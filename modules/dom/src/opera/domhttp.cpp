/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef DOM_HTTP_SUPPORT

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/opera/domhttp.h"
#include "modules/dom/src/domcore/node.h"
#include "modules/dom/src/domload/lsinput.h"
#include "modules/dom/src/domload/lsloader.h"
#include "modules/dom/src/domload/lsparser.h"
#include "modules/dom/src/domsave/lsoutput.h"
#include "modules/dom/src/domsave/lsserializer.h"
#include "modules/dom/src/domglobaldata.h"

#include "modules/doc/frm_doc.h"
#include "modules/encodings/encoders/outputconverter.h"
#include "modules/url/url2.h"
#include "modules/url/url_man.h"
#include "modules/util/str.h"
#include "modules/util/tempbuf.h"
#include "modules/formats/argsplit.h"
#include "modules/upload/upload.h"
#include "modules/dom/src/domfile/domblob.h"
#include "modules/dom/src/domfile/domfile.h"
#include "modules/url/tools/url_util.h"
#include "modules/url/protocols/http1.h"
#include "modules/forms/tempbuffer8.h"
#include "modules/dom/domutils.h"

#include "modules/security_manager/include/security_manager.h"
#include "modules/pi/OpSystemInfo.h"

DOM_HTTPRequest::DOM_HTTPRequest()
	: uri(NULL),
	  method(NULL),
	  input(NULL),
	  output(NULL),
	  inputstream(NULL),
	  outputstream(NULL),
	  headers(NULL),
	  report_304_verbatim(FALSE)
{
}

DOM_HTTPRequest::~DOM_HTTPRequest()
{
	OP_DELETEA(uri);
	OP_DELETEA(method);

	while (headers)
	{
		Header *header = headers;
		headers = header->next;

		OP_DELETE(header);
	}
}

#define NORMALISE_KNOWN_METHOD(x) if (uni_stri_eq(method, x)) return UNI_L(x)

static const uni_char*
NormaliseLegalMethod(const uni_char* method)
{
	/* case-insensitive matching of security-disallowed methods. */
	if (uni_stri_eq(method, "CONNECT") || uni_stri_eq(method, "TRACE") || uni_stri_eq(method, "TRACK"))
		return NULL;

	NORMALISE_KNOWN_METHOD("DELETE");
	NORMALISE_KNOWN_METHOD("GET");
	NORMALISE_KNOWN_METHOD("HEAD");
	NORMALISE_KNOWN_METHOD("OPTIONS");
	NORMALISE_KNOWN_METHOD("POST");
	NORMALISE_KNOWN_METHOD("PUT");

	return method;
}
#undef NORMALISE_KNOWN_METHOD

static BOOL
IsUploadMethod(const uni_char* method)
{
	return !(uni_stri_eq(method, "GET") ||
		   uni_stri_eq(method, "HEAD") ||
		   uni_stri_eq(method, "CONNECT") ||
		   uni_stri_eq(method, "TRACE") ||
		   uni_stri_eq(method, "TRACK"));
}

static char *
MunchWhitespace(char *str)
{
	while (*str)
		if (*str == '\t' || *str == ' ' || *str == '\r' || *str == '\n')
			str++;
		else
			return str;

	return NULL;
}

static char *
MunchTokenValue(char *token)
{
	while (*token)
		if (*token <= 0x20 || (*token) >= 0x7f || op_strchr("()<>@,;:\\\"/[]?={}", *token) != NULL)
			return token;
		else
			token++;

	return NULL;
}

/* static */ OP_STATUS
DOM_HTTPRequest::Make(DOM_HTTPRequest *&request, DOM_EnvironmentImpl *environment, const uni_char *uri, const uni_char *method)
{
	DOM_Runtime *runtime = environment->GetDOMRuntime();

	RETURN_IF_ERROR(DOMSetObjectRuntime(request = OP_NEW(DOM_HTTPRequest, ()), runtime, runtime->GetObjectPrototype(), "HTTPRequest"));
	RETURN_IF_ERROR(UniSetStr(request->uri, uri));
	RETURN_IF_ERROR(UniSetStr(request->method, method));

	RETURN_IF_ERROR(DOM_LSInput::Make(request->input, environment));
	RETURN_IF_ERROR(request->PutPrivate(DOM_PRIVATE_input, request->input));
	RETURN_IF_ERROR(DOM_HTTPInputStream::Make(request->inputstream, request));

	ES_Value value;
	DOMSetObject(&value, request->inputstream);
	RETURN_IF_ERROR(runtime->PutName(request->input, UNI_L("byteStream"), value));

	if (IsUploadMethod(method))
	{
		RETURN_IF_ERROR(DOM_LSOutput::Make(request->output, environment));
		RETURN_IF_ERROR(request->PutPrivate(DOM_PRIVATE_output, request->output));
		RETURN_IF_ERROR(DOM_HTTPOutputStream::Make(request->outputstream, request));

		ES_Value value;
		DOMSetObject(&value, request->outputstream);
		RETURN_IF_ERROR(runtime->PutName(request->output, UNI_L("byteStream"), value));
	}

	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_HTTPRequest::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_input:
		DOMSetObject(value, input);
		return GET_SUCCESS;

	case OP_ATOM_output:
		DOMSetObject(value, output);
		return GET_SUCCESS;

	case OP_ATOM_method:
		DOMSetString(value, method);
		return GET_SUCCESS;

	case OP_ATOM_uri:
		DOMSetString(value, uri);
		return GET_SUCCESS;
	}

	return GET_FAILED;
}

/* virtual */ ES_PutState
DOM_HTTPRequest::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (GetName(property_name, NULL, origining_runtime) == GET_SUCCESS)
		return PUT_SUCCESS;
	else
		return PUT_FAILED;
}

/* virtual */ void
DOM_HTTPRequest::GCTrace()
{
	GCMark(inputstream);
	GCMark(outputstream);
}

static OP_STATUS
SetURLMethod(URL& url, const uni_char* method)
{
	OpString8 method8;

	RETURN_IF_ERROR(method8.SetUTF8FromUTF16(method));

	return url.SetAttribute(URL::KHTTPSpecialMethodStr, method8);
}

static OP_STATUS
CopyIntoUploadBuffer(Upload_BinaryBuffer *upload, unsigned char *data, unsigned length, const char *mime_type, const char *encoding)
{
	TRAPD(status, upload->InitL(data, length, UPLOAD_COPY_BUFFER, mime_type, encoding, ENCODING_AUTODETECT));
	return status;
}

static OP_STATUS
CopyIntoUploadBinaryBuffer(Upload_BinaryBuffer *upload, unsigned char *data, unsigned length, const char *mime_type)
{
	TRAPD(status, upload->InitL(data, length, UPLOAD_TAKE_OVER_BUFFER, mime_type, 0, ENCODING_BINARY));
	return status;
}

/* static */ OP_STATUS
DOM_HTTPRequest::ConvertRangeHeaderInURLAttributes(OpString8 &header_value, URL &url, BOOL &attributes_set)
{
	HTTPRange http_range;

	attributes_set = FALSE;

	if (OpStatus::IsSuccess(http_range.Parse(header_value.CStr(), FALSE)))
	{
		OpFileLength start = http_range.GetStart();
		OpFileLength end = http_range.GetEnd();

		RETURN_IF_ERROR(url.SetAttribute(URL::KHTTPRangeStart, &start));
		RETURN_IF_ERROR(url.SetAttribute(URL::KHTTPRangeEnd, &end));

		attributes_set = TRUE;
	}

	return OpStatus::OK;
}

OP_STATUS
DOM_HTTPRequest::GetURL(URL &url, URL reference)
{
	BOOL unique = !uni_stri_eq(method, "GET");

	if (!unique && headers)
	{
		// Keep checking if the URL should be unique. In this case, the "Range" header is the trigger
		Header *header = headers;

		while (header && !unique)
		{
			// Ranges in an URL object should be managed with KHTTPRangeStart and KHTTPRangeEnd, not with the Range header
			if (!header->name.CompareI("Range"))
				unique = TRUE;

			header = header->next;
		}
	}

	url = g_url_api->GetURL(reference, uri, unique);

	RETURN_IF_ERROR(SetURLMethod(url, method));
	if (IsUploadMethod(method) && (outputstream->HasData() || outputstream->GetContentType()))
	{
		const unsigned char *src = outputstream->GetData();
		int src_len = outputstream->GetLength();
		BOOL is_binary = outputstream->GetIsBinary();

		if (DOM_Object *upload_object = outputstream->GetUploadObject())
		{
			if (upload_object->IsA(DOM_TYPE_FORMDATA))
			{
				DOM_FormData *formdata = static_cast<DOM_FormData *>(upload_object);

				Upload_Multipart *multipart;
				RETURN_IF_ERROR(formdata->Upload(multipart, outputstream->GetContentType(), FALSE));
				url.SetHTTP_Data(multipart, TRUE);
				outputstream->SetOutputLength(multipart->CalculateLength());
				outputstream->RelinquishData();
			}
			else if (upload_object->IsA(DOM_TYPE_FILE))
			{
				DOM_File *file = static_cast<DOM_File *>(upload_object);

				Upload_Handler *upload;
				RETURN_IF_ERROR(file->Upload(upload, NULL, NULL, outputstream->GetContentType(), TRUE));
				url.SetHTTP_Data(upload, TRUE);
				outputstream->SetOutputLength(upload->CalculateLength());
				outputstream->RelinquishData();
			}
			else
				OP_ASSERT(!"Unrecognised upload object type");
		}
		else
		{
			Upload_BinaryBuffer *upload = OP_NEW(Upload_BinaryBuffer, ());
			RETURN_OOM_IF_NULL(upload);
			OpAutoPtr<Upload_BinaryBuffer> upload_anchor(upload);

			if (is_binary)
			{
				const char *content_type = outputstream->GetContentType();
				if (!content_type)
					content_type = "application/octet-stream";
				RETURN_IF_ERROR(CopyIntoUploadBinaryBuffer(upload, const_cast<unsigned char *>(src), src_len, content_type));
				outputstream->RelinquishData();
			}
			else
			{
				const uni_char *encoding = NULL;
				OpString encoding_str;
				RETURN_IF_ERROR(DOM_LSOutput::GetEncoding(encoding, output, GetEnvironment()));
				if (encoding)
				{
					OpString8 encoding8;
					RETURN_IF_ERROR(encoding8.SetUTF8FromUTF16(encoding));
					RETURN_IF_ERROR(outputstream->UpdateContentType(NULL, encoding8.CStr()));
				}
				else
				{
					RETURN_IF_ERROR(outputstream->GetContentEncoding(encoding_str));
					encoding = encoding_str.CStr();
				}

				OutputConverter *converter;
				RETURN_IF_ERROR(OutputConverter::CreateCharConverter(encoding, &converter));
				OpAutoPtr<OutputConverter> converter_anchor(converter);

				TempBuffer buffer;
				unsigned char *dest;
				int dest_len = 0, read = 0, src_ptr = 0;
				int dest_ptr = 0;

				while (src_ptr < src_len)
				{
					RETURN_IF_ERROR(buffer.Expand(dest_len > src_len ? dest_len + 256 : src_len));
					dest = reinterpret_cast<unsigned char *>(buffer.GetStorage());
					dest_len = buffer.GetCapacity() * sizeof(uni_char);

					int result = converter->Convert(src + src_ptr, src_len - src_ptr, dest + dest_ptr, dest_len - dest_ptr, &read);
					if (result < 0)
						return OpStatus::ERR_NO_MEMORY;

					src_ptr += read;
					dest_ptr += result;
				}

				RETURN_IF_ERROR(CopyIntoUploadBuffer(upload, reinterpret_cast<unsigned char *>(buffer.GetStorage()), static_cast<unsigned>(dest_ptr), outputstream->GetContentType(), 0));
			}

			RETURN_IF_LEAVE(upload->PrepareUploadL(UPLOAD_BINARY_NO_CONVERSION));
			url.SetHTTP_Data(upload_anchor.release(), TRUE);
		}
	}

	if (headers)
	{
		Header *header = headers;

		while (header)
		{
			BOOL bypass_header = FALSE;

			// If the Range header is present (which implies that unique is TRUE), set the proper attributes
			if (unique && !header->name.CompareI("Range"))
				RETURN_IF_ERROR(ConvertRangeHeaderInURLAttributes(header->value, url, bypass_header));

			if (!bypass_header)
			{
				URL_Custom_Header header_item;

				RETURN_IF_ERROR(header_item.name.Set(header->name));
				RETURN_IF_ERROR(header_item.value.Set(header->value));
				/* Disable security checks for this Range header addition; we do want it. */
				header_item.bypass_security = TRUE;

				RETURN_IF_ERROR(url.SetAttribute(URL::KAddHTTPHeader, &header_item));
			}

			header = header->next;
		}
	}

	return OpStatus::OK;
}

OP_STATUS
DOM_HTTPRequest::AddHeader(const uni_char *name, const uni_char *value)
{
	if (uni_stri_eq(name, "ACCEPT-CHARSET") ||
	    uni_stri_eq(name, "ACCEPT-ENCODING") ||
	    uni_stri_eq(name, "ACCESS-CONTROL-REQUEST-HEADERS") ||
	    uni_stri_eq(name, "ACCESS-CONTROL-REQUEST-METHOD") ||
	    uni_stri_eq(name, "CONNECTION") ||
	    uni_stri_eq(name, "CONTENT-LENGTH") ||
	    uni_stri_eq(name, "COOKIE") ||
	    uni_stri_eq(name, "COOKIE2") ||
	    uni_stri_eq(name, "DATE") ||
	    uni_stri_eq(name, "DNT") ||
	    uni_stri_eq(name, "EXPECT") ||
	    uni_stri_eq(name, "HOST") ||
	    uni_stri_eq(name, "KEEP-ALIVE") ||
	    uni_stri_eq(name, "ORIGIN") ||
	    uni_stri_eq(name, "REFERER") ||
	    uni_stri_eq(name, "TE") ||
	    uni_stri_eq(name, "TRAILER") ||
	    uni_stri_eq(name, "TRANSFER-ENCODING") ||
	    uni_stri_eq(name, "UPGRADE") ||
	    uni_stri_eq(name, "USER-AGENT") ||
	    uni_stri_eq(name, "VIA"))

		return OpStatus::OK;

	if (uni_strnicmp(name, "PROXY-", 6) == 0 || uni_strnicmp(name, "SEC-", 4) == 0)
		return OpStatus::OK;

	OpString8 value8;
	RETURN_IF_ERROR(value8.Set(value));

	if (uni_stri_eq(name, "CONTENT-TYPE") && IsUploadMethod(method))
	{
		RETURN_IF_ERROR(outputstream->SetContentType(value8));
		return OpStatus::OK;
	}

	OpString8 name8;
	RETURN_IF_ERROR(name8.Set(name));

	Header *header = NULL, **ptr = &headers;

	while (*ptr)
	{
		if ((*ptr)->name.CompareI(name8) == 0)
		{
			header = *ptr;
			break;
		}

		ptr = &(*ptr)->next;
	}

	if (!header)
	{
		RETURN_OOM_IF_NULL(header = OP_NEW(Header, ()));
		RETURN_IF_ERROR(header->name.Set(name8));
		RETURN_IF_ERROR(header->value.Set(value8));

		header->next = NULL;
		*ptr = header;
	}
	else
	{
		if (uni_stri_eq(name, "AUTHORIZATION") ||
			uni_stri_eq(name, "CONTENT-BASE") ||
			uni_stri_eq(name, "CONTENT-LOCATION") ||
			uni_stri_eq(name, "CONTENT-MD5") ||
			uni_stri_eq(name, "CONTENT-RANGE") ||
			uni_stri_eq(name, "CONTENT-TYPE") ||
			uni_stri_eq(name, "CONTENT-VERSION") ||
			uni_stri_eq(name, "DELTA-BASE") ||
			uni_stri_eq(name, "DEPTH") ||
			uni_stri_eq(name, "DESTINATION") ||
			uni_stri_eq(name, "ETAG") ||
			uni_stri_eq(name, "FROM") ||
			uni_stri_eq(name, "IF-MODIFIED-SINCE") ||
			uni_stri_eq(name, "IF-RANGE") ||
			uni_stri_eq(name, "IF-UNMODIFIED-SINCE") ||
			uni_stri_eq(name, "MAX-FORWARDS") ||
			uni_stri_eq(name, "MIME-VERSION") ||
			uni_stri_eq(name, "OVERWRITE") ||
			uni_stri_eq(name, "PROXY-AUTHORIZATION") ||
			uni_stri_eq(name, "SOAPACTION") ||
			uni_stri_eq(name, "TIMEOUT"))
			RETURN_IF_ERROR(header->value.Set(value8));
		else
		{
			RETURN_IF_ERROR(header->value.Append(", "));
			RETURN_IF_ERROR(header->value.Append(value8));
		}

		if (!report_304_verbatim)
			if (uni_stri_eq(name, "IF-MATCH") ||
			    uni_stri_eq(name, "IF-MODIFIED-SINCE") ||
			    uni_stri_eq(name, "IF-NONE-MATCH") ||
			    uni_stri_eq(name, "IF-RANGE") ||
			    uni_stri_eq(name, "IF-UNMODIFIED-SINCE"))
				report_304_verbatim = TRUE;
	}

	return OpStatus::OK;
}

OP_STATUS
DOM_HTTPRequest::ForceContentType(const uni_char *content_type)
{
	RETURN_IF_LEAVE(forced_response_type.SetUTF8FromUTF16L(content_type));
	return OpStatus::OK;
}

DOM_HTTPInputStream::DOM_HTTPInputStream(DOM_HTTPRequest *request)
	: request(request)
{
}

/* static */ OP_STATUS
DOM_HTTPInputStream::Make(DOM_HTTPInputStream *&inputstream, DOM_HTTPRequest *request)
{
	DOM_Runtime *runtime = request->GetRuntime();
	return DOMSetObjectRuntime(inputstream = OP_NEW(DOM_HTTPInputStream, (request)), runtime, runtime->GetObjectPrototype(), "HTTPInputStream");
}

/* virtual */ void
DOM_HTTPInputStream::GCTrace()
{
	GCMark(request);
}

DOM_HTTPOutputStream::DOM_HTTPOutputStream(DOM_HTTPRequest *request)
	: request(request),
	  data(NULL),
	  length(0),
	  upload_object(NULL),
	  upload_length(0),
	  content_type(NULL)
{
}

DOM_HTTPOutputStream::~DOM_HTTPOutputStream()
{
	OP_DELETEA(data);
	OP_DELETEA(content_type);
}

/* static */ OP_STATUS
DOM_HTTPOutputStream::Make(DOM_HTTPOutputStream *&outputstream, DOM_HTTPRequest *request)
{
	DOM_Runtime *runtime = request->GetRuntime();
	return DOMSetObjectRuntime(outputstream = OP_NEW(DOM_HTTPOutputStream, (request)), runtime, runtime->GetObjectPrototype(), "HTTPOutputStream");
}

void
DOM_HTTPOutputStream::RelinquishData()
{
	data = NULL;
	if (upload_length == 0)
		upload_length = length;
	length = 0;
	upload_object = NULL;
}

/* virtual */ void
DOM_HTTPOutputStream::GCTrace()
{
	GCMark(request);
	GCMark(upload_object);
}

OP_STATUS
DOM_HTTPOutputStream::SetData(const unsigned char *new_data, unsigned new_length, BOOL new_is_binary, DOM_Object *external_object)
{
	OP_DELETEA(data);
	if (!external_object)
	{
		OP_ASSERT(new_data || new_length == 0);
		if (new_data)
		{
			RETURN_OOM_IF_NULL(data = OP_NEWA(unsigned char, new_length + 1));
			op_memcpy(data, new_data, new_length);
			data[new_length] = 0;
		}
		length = new_length;
		is_binary = new_is_binary;
		upload_object = NULL;
		upload_length = 0;
	}
	else
	{
		upload_object = external_object;
		upload_length = 0;
		data = NULL;
		OP_ASSERT(new_length == 0);
		length = 0;
		is_binary = TRUE;
	}
	return OpStatus::OK;
}

void
DOM_HTTPOutputStream::GetContentEncodingL(OpString &result)
{
	if (!content_type)
	{
		result.SetL("UTF-8");
		return;
	}

	ParameterList mime_type_parts;
	ANCHOR(ParameterList, mime_type_parts);

	/* Be liberal as can be; if setRequestHeader() starts canonicalizing, the flags
	   here can be made to follow. */
	unsigned flags = PARAM_SEP_SEMICOLON | PARAM_SEP_WHITESPACE | PARAM_STRIP_ARG_QUOTES | PARAM_SEP_COMMA | NVS_COOKIE_SEPARATION | PARAM_HAS_RFC2231_VALUES;
	mime_type_parts.SetValueL(content_type, flags);
	if (Parameters* item = mime_type_parts.First())
		for (item = item->Suc(); item; item = item->Suc())
		{
			const char* param_name = item->Name();
			if (param_name && op_stricmp(param_name, "charset") == 0)
			{
				result.SetL(item->Value());
				return;
			}
		}

	result.SetL("UTF-8");
}

OP_STATUS
DOM_HTTPOutputStream::GetContentEncoding(OpString &result)
{
	TRAPD(status, GetContentEncodingL(result));
	return status;
}

OP_STATUS
DOM_HTTPOutputStream::SetContentType(const char *mime_type)
{
	OP_ASSERT(mime_type);
	OP_DELETEA(content_type);
	content_type = SetNewStr(mime_type);
	RETURN_OOM_IF_NULL(content_type);
	return OpStatus::OK;
}

OP_STATUS
DOM_HTTPOutputStream::UpdateContentType(const uni_char *mime_type, const char *encoding)
{
	OpString8 str8;
	RETURN_IF_ERROR(str8.SetUTF8FromUTF16(mime_type));
	return TransformContentType(content_type, str8.CStr(), encoding);
}

/* static */ OP_STATUS
DOM_HTTPOutputStream::TransformContentType(char *&content_type, const char *mime_type, const char *encoding)
{
	if (content_type && encoding)
	{
		/* Special case (and then some): setRequestHeader("Content-Type", x) has already
		   set the type. If it is a valid MIME media type and one or more the charset='s
		   aren't a case-insensitive match for 'encoding', then replace all. If not, leave
		   the content type undisturbed. */
		TempBuffer8 new_content_type;
		unsigned length_encoding = op_strlen(encoding);

		char *content_type_start = content_type;
		char *content_type_ref = content_type;

		BOOL replace_all = FALSE;

		content_type_start = MunchTokenValue(content_type_start);

		/* If not a valid MIME type, do not replace. */
		if (!content_type_start || *content_type_start != '/' || (content_type_start = MunchTokenValue(content_type_start + 1)) == NULL)
			return OpStatus::OK;

		if ((content_type_start = MunchWhitespace(content_type_start)) == NULL || *content_type_start != ';')
			return OpStatus::OK;

again:
		while (*content_type_start && *content_type_start != ';')
			content_type_start++;

		if (*content_type_start == ';')
		{
			while (content_type_start && *content_type_start)
			{
				content_type_start = MunchWhitespace(++content_type_start);
				if (!content_type_start)
					break;
				char *param_token_end = MunchTokenValue(content_type_start);
				param_token_end = param_token_end ? MunchWhitespace(param_token_end) : NULL;
				if (param_token_end && *param_token_end == '=')
				{
					param_token_end++;
					if (uni_strnicmp(content_type_start, UNI_L("charset"), 7) == 0 && (content_type_start[7] == '=' || op_isspace(content_type_start[7])))
					{
						char *charset_start = MunchWhitespace(param_token_end);
						BOOL substitute = TRUE;
						if (charset_start && *charset_start == '"')
						{
							substitute = op_strnicmp(charset_start + 1, encoding, length_encoding) != 0;
							if (!substitute && charset_start[1 + length_encoding] != '"')
								substitute = TRUE;
						}
						else if (charset_start)
							substitute = op_strnicmp(charset_start, encoding, length_encoding) != 0;

						if (!replace_all && substitute)
						{
							replace_all = TRUE;
							content_type_start = content_type_ref = content_type;
							goto again;
						}

						if (replace_all || substitute)
						{
							RETURN_IF_ERROR(new_content_type.Append(content_type_ref, param_token_end - content_type_ref));
							RETURN_IF_ERROR(new_content_type.Append(encoding));
							content_type_ref = NULL;
							if (charset_start)
								param_token_end = charset_start;
						}
					}

					/* Skip value. */
					if (*param_token_end == '"')
					{
						param_token_end++;
						while (*param_token_end)
							if (*param_token_end == '\\' && param_token_end[1] == '"')
								param_token_end += 2;
							else if (*param_token_end++ == '"')
								break;
					}
					else
						param_token_end = MunchTokenValue(param_token_end);

					if (!content_type_ref)
						content_type_ref = param_token_end;
				}
				else if (param_token_end)
					param_token_end++;

				content_type_start = param_token_end;
			}
			if (content_type_ref)
				RETURN_IF_ERROR(new_content_type.Append(content_type_ref, content_type_start ? (content_type_start - content_type_ref) : op_strlen(content_type_ref)));

			OP_DELETEA(content_type);
			content_type = SetNewStr(new_content_type.GetStorage());
			RETURN_OOM_IF_NULL(content_type);
		}
	}
	else if (!content_type && mime_type)
	{
		content_type = SetNewStr(mime_type);
		RETURN_OOM_IF_NULL(content_type);
	}

	return OpStatus::OK;
}

OpFileLength
DOM_HTTPOutputStream::GetContentLength()
{
	if (upload_length > 0)
		return upload_length;
	else
		return is_binary ? static_cast<OpFileLength>(length) : static_cast<OpFileLength>(length / sizeof(uni_char));
}

DOM_XMLHttpRequest::ResponseDataBuffer::~ResponseDataBuffer()
{
	op_free(m_buffer);
	op_free(m_byte_buffer);
}

void
DOM_XMLHttpRequest::ResponseDataBuffer::SetIsByteBuffer(BOOL f)
{
	OP_ASSERT(f ? m_buffer == NULL : m_byte_buffer == NULL);
	m_is_byte_buffer = f;
}

void
DOM_XMLHttpRequest::ResponseDataBuffer::Clear()
{
	op_free(m_buffer);
	m_buffer = NULL;

	op_free(m_byte_buffer);
	m_byte_buffer = NULL;

	m_used = 0;
	m_buffer_size = 0;
	m_is_byte_buffer = FALSE;
}

void
DOM_XMLHttpRequest::ResponseDataBuffer::ClearByteBuffer(BOOL release)
{
	if (release)
		op_free(m_byte_buffer);
	m_byte_buffer = NULL;
}

OP_STATUS
DOM_XMLHttpRequest::ResponseDataBuffer::GrowBuffer(unsigned extra_bytes)
{
	if (extra_bytes > 0 && ((m_is_byte_buffer ? !m_byte_buffer : !m_buffer) || m_used + extra_bytes > m_buffer_size))
	{
		unsigned new_size = m_buffer_size == 0 ? 256 : 2 * m_buffer_size;
		if (new_size < m_buffer_size)
			return OpStatus::ERR;
		while (new_size < m_used + extra_bytes)
		{
			unsigned double_up = new_size * 2;
			if (double_up < new_size)
				return OpStatus::ERR;
			new_size = double_up;
		}

		if (m_is_byte_buffer)
		{
			unsigned char *new_buffer = static_cast<unsigned char *>(op_realloc(m_byte_buffer, new_size));
			RETURN_OOM_IF_NULL(new_buffer);
			m_byte_buffer = new_buffer;
		}
		else
		{
			uni_char *new_buffer = static_cast<uni_char *>(op_realloc(m_buffer, new_size * sizeof(uni_char)));
			RETURN_OOM_IF_NULL(new_buffer);
			m_buffer = new_buffer;
		}
		m_buffer_size = new_size;
	}

	return OpStatus::OK;
}

OP_STATUS
DOM_XMLHttpRequest::ResponseDataBuffer::Append(const uni_char* data, unsigned length)
{
	OP_ASSERT(!m_is_byte_buffer && m_byte_buffer == NULL);
	RETURN_IF_ERROR(GrowBuffer(length));
	op_memcpy(m_buffer + m_used, data, length * sizeof(uni_char));
	m_used += length;

	return OpStatus::OK;
}

OP_STATUS
DOM_XMLHttpRequest::ResponseDataBuffer::AppendBytes(const unsigned char* data, unsigned length)
{
	OP_ASSERT(m_is_byte_buffer && m_buffer == NULL);
	RETURN_IF_ERROR(GrowBuffer(length));
	op_memcpy(m_byte_buffer + m_used, data, length);
	m_used += length;

	return OpStatus::OK;
}

DOM_XMLHttpRequest::DOM_XMLHttpRequest(BOOL is_anon)
	: status(0),
	  readyState(READYSTATE_UNSENT),
	  queued_readyState(READYSTATE_UNINITIALIZED),
	  request(NULL),
	  parser(NULL),
	  responseXML(NULL),
	  responseBuffer(NULL),
	  responseBlob(NULL),
	  responseType(NULL),
	  responseHeaders(NULL),
	  readystatechanges_in_progress(0),
	  async(TRUE),
	  send_flag(FALSE),
	  error_flag(FALSE),
	  request_error_status(NoError),
	  is_anonymous(is_anon),
#ifdef PROGRESS_EVENTS_SUPPORT
	  uploader(NULL),
	  progress_events_flag(FALSE),
	  upload_complete_flag(TRUE),
	  timeout_value(0),
	  delayed_progress_event_pending(FALSE),
	  last_progress_event_time(0.0),
	  downloaded_bytes(0),
	  last_downloaded_bytes(0),
	  delivered_onloadstart(FALSE),
#endif // PROGRESS_EVENTS_SUPPORT
	  blocked_on_prepare_upload(FALSE),
	  username(NULL),
	  password(NULL),
#ifdef CORS_SUPPORT
	  with_credentials(FALSE),
	  want_cors_request(FALSE),
	  is_cors_request(FALSE),
	  cross_origin_request(NULL),
#endif // CORS_SUPPORT
	  waiting_for_headers_thread_listener(this),
	  headers_received(FALSE)
{
}

OP_STATUS
DOM_XMLHttpRequest::AdvanceReadyStateTowardsActualReadyState(ES_Thread *interrupt_thread)
{
	if (queued_readyState != READYSTATE_UNINITIALIZED)
		return SetReadyState(readyState < queued_readyState ? static_cast<ReadyState>(readyState + 1) : READYSTATE_OPEN, interrupt_thread);
	return OpStatus::OK;
}

DOM_XMLHttpRequest::ReadyState
DOM_XMLHttpRequest::GetActualReadyState()
{
	if (queued_readyState != READYSTATE_UNINITIALIZED)
		return queued_readyState;
	else
		return readyState;
}

/* static */ OP_STATUS
DOM_XMLHttpRequest::Make(DOM_XMLHttpRequest *&xmlhttprequest, DOM_EnvironmentImpl *environment, BOOL is_anonymous)
{
	DOM_Runtime *runtime = environment->GetDOMRuntime();
	return DOMSetObjectRuntime(xmlhttprequest = OP_NEW(DOM_XMLHttpRequest, (is_anonymous)), runtime, runtime->GetPrototype(DOM_Runtime::XMLHTTPREQUEST_PROTOTYPE), is_anonymous ? "AnonXMLHttpRequest" : "XMLHttpRequest");
}

DOM_XMLHttpRequest::~DOM_XMLHttpRequest()
{
	OP_DELETEA(responseType);
	OP_DELETEA(responseHeaders);
	OP_DELETEA(username);
	OP_DELETEA(password);
#ifdef CORS_SUPPORT
	OP_DELETE(cross_origin_request);
#endif // CORS_SUPPORT
}

/* static */ void
DOM_XMLHttpRequest::ConstructXMLHttpRequestObjectL(ES_Object *object, DOM_Runtime *runtime)
{
	PutNumericConstantL(object, "UNSENT", READYSTATE_UNSENT, runtime);
	PutNumericConstantL(object, "OPENED", READYSTATE_OPENED, runtime);
	PutNumericConstantL(object, "HEADERS_RECEIVED", READYSTATE_HEADERS_RECEIVED, runtime);
	PutNumericConstantL(object, "LOADING", READYSTATE_LOADING, runtime);
	PutNumericConstantL(object, "DONE", READYSTATE_DONE, runtime);
}

/* virtual */ ES_GetState
DOM_XMLHttpRequest::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_status:
		if (error_flag || readyState < READYSTATE_HEADERS_RECEIVED)
			DOMSetNumber(value, 0);
		else
			DOMSetNumber(value, status);
		return GET_SUCCESS;

	case OP_ATOM_statusText:
		if (error_flag || readyState < READYSTATE_HEADERS_RECEIVED)
			DOMSetString(value);
		else
			DOMSetString(value, statusText.CStr());
		return GET_SUCCESS;

	case OP_ATOM_readyState:
		DOMSetNumber(value, readyState);
		return GET_SUCCESS;

	case OP_ATOM_responseType:
		DOMSetString(value, responseType);
		return GET_SUCCESS;

	case OP_ATOM_response:
		if (error_flag || readyState < READYSTATE_RECEIVING)
			if (!responseType || !*responseType || uni_str_eq(responseType, "text"))
				DOMSetString(value);
			else
				DOMSetNull(value);
		else if (value)
			if (!responseType || !*responseType || uni_str_eq(responseType, "text"))
				DOMSetStringWithLength(value, &(g_DOM_globalData->string_with_length_holder), responseData.GetStorage(), responseData.Length());
			else if (uni_str_eq(responseType, "json"))
				GET_FAILED_IF_ERROR(ToJSON(value, origining_runtime));
			else if (responseData.IsByteBuffer())
				if (uni_str_eq(responseType, "arraybuffer"))
				{
					if (!responseBuffer)
					{
						unsigned length = responseData.Length();
						unsigned char *byte_storage = responseData.GetByteStorage();
						GET_FAILED_IF_ERROR(GetRuntime()->CreateNativeArrayBufferObject(&responseBuffer, length, byte_storage));
						responseData.ClearByteBuffer(FALSE);
					}
					DOMSetObject(value, responseBuffer);
				}
				else
				{
					OP_ASSERT(responseType && uni_str_eq(responseType, "blob"));
					if (!responseBlob)
					{
						unsigned length = responseData.Length();
						unsigned char *byte_storage = responseData.GetByteStorage();
						GET_FAILED_IF_ERROR(DOM_Blob::Make(responseBlob, byte_storage, length, GetRuntime()));
						responseData.ClearByteBuffer(FALSE);
					}
					DOMSetObject(value, responseBlob);
				}
			else if (uni_str_eq(responseType, "document"))
				DOMSetObject(value, responseXML);
			else
			{
				OP_ASSERT(!"Unknown response type, cannot happen");
				DOMSetNull(value);
			}

		return GET_SUCCESS;

	case OP_ATOM_responseText:
		if (error_flag || readyState < READYSTATE_RECEIVING)
			DOMSetString(value);
		else if (value)
			if (!responseType || !*responseType || uni_stri_eq(responseType, "text"))
				DOMSetStringWithLength(value, &(g_DOM_globalData->string_with_length_holder), responseData.GetStorage(), responseData.Length());
			else
				return GetNameDOMException(INVALID_STATE_ERR, value);

		return GET_SUCCESS;

	case OP_ATOM_responseXML:
		if (error_flag || readyState < READYSTATE_RECEIVING)
			DOMSetNull(value);
		else if (value)
		{
			if (!responseType || uni_stri_eq(responseType, "document"))
				DOMSetObject(value, responseXML);
			else
				return GetNameDOMException(INVALID_STATE_ERR, value);
		}
		return GET_SUCCESS;

#ifdef CORS_SUPPORT
	case OP_ATOM_withCredentials:
		DOMSetBoolean(value, with_credentials);
		return GET_SUCCESS;
#endif // CORS_SUPPORT

#ifdef PROGRESS_EVENTS_SUPPORT
	case OP_ATOM_timeout:
		DOMSetNumber(value, timeout_value);
		return GET_SUCCESS;

	case OP_ATOM_upload:
		if (value && !uploader)
			GET_FAILED_IF_ERROR(DOM_XMLHttpRequestUpload::Make(uploader, GetRuntime()));
		DOMSetObject(value, uploader);
		return GET_SUCCESS;
#endif // PROGRESS_EVENTS_SUPPORT
	}

	return GET_FAILED;
}

/* virtual */ ES_GetState
DOM_XMLHttpRequest::GetName(const uni_char *property_name, int property_code, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_name[0] == 'o' && property_name[1] == 'n')
	{
		ES_GetState res = GetEventProperty(property_name, value, static_cast<DOM_Runtime*>(origining_runtime));
		if (res != GET_FAILED)
			return res;
	}
	return DOM_Object::GetName(property_name, property_code, value, origining_runtime);
}

/* The range of 'XHR.responseType's currently supported. */
static BOOL
IsSupportedResponseType(const uni_char *type_string, BOOL &is_binary_type)
{
	if (*type_string == 0 || uni_str_eq(type_string, "text") || uni_str_eq(type_string, "document") || uni_str_eq(type_string, "json"))
	{
		is_binary_type = FALSE;
		return TRUE;
	}
	else if (uni_str_eq(type_string, "arraybuffer") || uni_str_eq(type_string, "blob"))
	{
		is_binary_type = TRUE;
		return TRUE;
	}
	else
		return FALSE;
}

/* virtual */ ES_PutState
DOM_XMLHttpRequest::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_responseType:
		if (GetActualReadyState() >= READYSTATE_RECEIVING)
			return PutNameDOMException(INVALID_STATE_ERR, value);
#ifdef DOM_XMLHTTPREQUEST_CONSTRAIN_SYNC_USAGE
		else if (HasWindowDocument() && !async)
			return PutNameDOMException(INVALID_ACCESS_ERR, value);
#endif // DOM_XMLHTTPREQUEST_CONSTRAIN_SYNC_USAGE
		else if (value->type != VALUE_STRING)
			return PUT_NEEDS_STRING;
		else
		{
			BOOL is_binary_type = FALSE;
			if (!IsSupportedResponseType(value->value.string, is_binary_type))
				return PUT_SUCCESS;

			OP_DELETEA(responseType);
			responseType = UniSetNewStr(value->value.string);
			if (!responseType)
				return PUT_NO_MEMORY;
			if (responseData.IsByteBuffer() != is_binary_type)
			{
				const uni_char *override_content_type = is_binary_type ? UNI_L("application/octet-stream") : NULL;
				PUT_FAILED_IF_ERROR(forced_content_type.Set(override_content_type));
				if (readyState == READYSTATE_OPENED)
					PUT_FAILED_IF_ERROR(request->ForceContentType(override_content_type));
				responseData.SetIsByteBuffer(is_binary_type);
				if (parser)
					parser->SetParseLoadedData(!is_binary_type);
			}
		}
		return PUT_SUCCESS;

#ifdef CORS_SUPPORT
	case OP_ATOM_withCredentials:
		if (GetActualReadyState() >= READYSTATE_SENT || send_flag)
			return PutNameDOMException(INVALID_STATE_ERR, value);
		else if (is_anonymous)
			return PutNameDOMException(INVALID_ACCESS_ERR, value);
#ifdef DOM_XMLHTTPREQUEST_CONSTRAIN_SYNC_USAGE
		else if (HasWindowDocument() && !async)
			return PutNameDOMException(INVALID_ACCESS_ERR, value);
#endif // DOM_XMLHTTPREQUEST_CONSTRAIN_SYNC_USAGE
		else if (value->type != VALUE_BOOLEAN)
			return PUT_NEEDS_BOOLEAN;
		else
		{
			with_credentials = value->value.boolean;
			return PUT_SUCCESS;
		}
#endif // CORS_SUPPORT

#ifdef PROGRESS_EVENTS_SUPPORT
	case OP_ATOM_timeout:
		if (value->type != VALUE_NUMBER)
			return PUT_NEEDS_NUMBER;
#ifdef DOM_XMLHTTPREQUEST_CONSTRAIN_SYNC_USAGE
		else if (HasWindowDocument() && !async)
			return PutNameDOMException(INVALID_ACCESS_ERR, value);
#endif // DOM_XMLHTTPREQUEST_CONSTRAIN_SYNC_USAGE
		else
		{
			timeout_value = TruncateDoubleToUInt(value->value.number);
			return PUT_SUCCESS;
		}
#endif // PROGRESS_EVENTS_SUPPORT

	case OP_ATOM_readyState:
	case OP_ATOM_status:
	case OP_ATOM_statusText:
	case OP_ATOM_response:
	case OP_ATOM_responseText:
	case OP_ATOM_responseXML:
#ifdef PROGRESS_EVENTS_SUPPORT
	case OP_ATOM_upload:
#endif // PROGRESS_EVENTS_SUPPORT
		return PUT_READ_ONLY;

	default:
		if (GetName(property_name, NULL, origining_runtime) != GET_FAILED)
			return PUT_SUCCESS;
		else
			return PUT_FAILED;
	}
}

/* virtual */ ES_PutState
DOM_XMLHttpRequest::PutName(const uni_char *property_name, int property_code, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_name[0] == 'o' && property_name[1] == 'n')
	{
		ES_PutState res = PutEventProperty(property_name, value, static_cast<DOM_Runtime*>(origining_runtime));
		if (res != PUT_FAILED)
			return res;
	}
	return DOM_Object::PutName(property_name, property_code, value, origining_runtime);
}

/* virtual */ void
DOM_XMLHttpRequest::GCTrace()
{
	GCMark(event_target);
	GCMark(request);
	GCMark(parser);
#ifdef PROGRESS_EVENTS_SUPPORT
	GCMark(uploader);
#endif // PROGRESS_EVENTS_SUPPORT
	GCMark(responseXML);
	GCMark(responseBuffer);
	GCMark(responseBlob);
}

/* virtual */ ES_GetState
DOM_XMLHttpRequest::FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime *origining_runtime)
{
	ES_GetState result = DOM_Object::FetchPropertiesL(enumerator, origining_runtime);
	if (result != GET_SUCCESS)
		return result;

	/* The event properties are not found as atoms */
	enumerator->AddPropertyL("onreadystatechange");
	enumerator->AddPropertyL("onload");
	enumerator->AddPropertyL("onerror");
#ifdef PROGRESS_EVENTS_SUPPORT
	enumerator->AddPropertyL("onloadstart");
	enumerator->AddPropertyL("onloadend");
	enumerator->AddPropertyL("onprogress");
	enumerator->AddPropertyL("onabort");
	enumerator->AddPropertyL("ontimeout");
#endif // PROGRESS_EVENTS_SUPPORT

	return GET_SUCCESS;
}

OP_STATUS
DOM_XMLHttpRequest::SetStatus(const URL &url)
{
	status = url.GetAttribute(URL::KHTTP_Response_Code, TRUE);

	if (status == HTTP_NOT_MODIFIED)
		return SetNotModifiedStatus(url);

	OpString8 statusText8;
	RETURN_IF_ERROR(url.GetAttribute(URL::KHTTPResponseText, statusText8, TRUE));
	return statusText.Set(statusText8);
}

OP_STATUS
DOM_XMLHttpRequest::SetNotModifiedStatus(const URL &url)
{
	if (request && request->ReportNotModifiedStatus())
	{
		status = HTTP_NOT_MODIFIED;
		return statusText.Set("Not Modified");
	}
	else
	{
		status = HTTP_OK;
		return statusText.Set("OK");
	}
}

OP_STATUS
DOM_XMLHttpRequest::SetCredentials(URL &url)
{
#ifdef CORS_SUPPORT
	/* A request needs to explicitly opt in on using credentials
	   for cross-origin requests. */
	if (want_cors_request && !with_credentials)
		return url.SetAttribute(URL::KDisableProcessCookies, TRUE);
	else
	{
		RETURN_IF_ERROR(url.SetAttribute(URL::KDisableProcessCookies, FALSE));
		BOOL allow_simple_cors_redirects = !want_cors_request;

# if defined(GADGET_SUPPORT) && !defined(SECMAN_CROSS_ORIGIN_XHR_GADGET_SUPPORT)
		/* Gadgets do not participate in CORS. */
		if (allow_simple_cors_redirects)
			if (FramesDocument *frames_doc = GetEnvironment()->GetFramesDocument())
				if (frames_doc->GetWindow()->GetType() == WIN_TYPE_GADGET)
					allow_simple_cors_redirects = FALSE;
# endif // GADGET_SUPPORT && !SECMAN_CROSS_ORIGIN_XHR_GADGET_SUPPORT

		if (allow_simple_cors_redirects)
		{
			/* Indicate if redirects away from same-origin are allowed. Only
			   support these if the request is a non-preflighted (and simple)
			   CORS request. */
			BOOL is_simple = OpCrossOrigin_Request::IsSimpleMethod(GetMethod());
			DOM_HTTPRequest::Header *header = request->GetFirstHeader();
			while (is_simple && header)
			{
				is_simple = OpCrossOrigin_Request::IsSimpleHeader(header->name.CStr(), header->value.CStr());
				header = header->next;
			}
			if (is_simple)
				if (DOM_HTTPOutputStream *outputstream = request->GetOutputStream())
					if (outputstream->GetContentType())
						is_simple = OpCrossOrigin_Request::IsSimpleHeader("Content-Type", outputstream->GetContentType());

			unsigned redirect_setting = is_simple ? URL::CORSRedirectAllowSimpleCrossOrigin : URL::CORSRedirectDenySimpleCrossOrigin;
			if (is_simple && is_anonymous)
				redirect_setting = URL::CORSRedirectAllowSimpleCrossOriginAnon;

			RETURN_IF_ERROR(url.SetAttribute(URL::KFollowCORSRedirectRules, redirect_setting));
		}
	}
#endif // CORS_SUPPORT

	if (username && password)
	{
		OpString8 username8;
		RETURN_IF_ERROR(username8.SetUTF8FromUTF16(username));
		OpString8 password8;
		RETURN_IF_ERROR(password8.SetUTF8FromUTF16(password));

		RETURN_IF_ERROR(url.SetAttribute(URL::KHTTPUsername, username8));
		RETURN_IF_ERROR(url.SetAttribute(URL::KHTTPPassword, password8));
	}

	return OpStatus::OK;
}

class DOM_XHR_ReadyStateChangeListener
	: public ES_ThreadListener
{
public:
	DOM_XHR_ReadyStateChangeListener(DOM_XMLHttpRequest *xhr, ES_Thread *interrupt_thread)
		: xmlhttprequest(xhr)
		, interrupt_thread(interrupt_thread)
	{
	}

	~DOM_XHR_ReadyStateChangeListener()
	{
		ES_ThreadListener::Remove();
	}

	virtual OP_STATUS Signal(ES_Thread *thread, ES_ThreadSignal signal)
	{
		OP_ASSERT(xmlhttprequest->readystatechanges_in_progress > 0);
		xmlhttprequest->readystatechanges_in_progress--;
		OP_STATUS status = xmlhttprequest->AdvanceReadyStateTowardsActualReadyState(interrupt_thread);
		OP_DELETE(this);
		return status;
	}

private:
	DOM_XMLHttpRequest *xmlhttprequest;
	ES_Thread *interrupt_thread;
};

OP_STATUS
DOM_XMLHttpRequest::SetReadyState(ReadyState new_readyState, ES_Thread *interrupt_thread, BOOL force_immediate_switch)
{
	if (readyState != new_readyState)
	{
		if (readystatechanges_in_progress > 0 && !force_immediate_switch)
		{
			if (queued_readyState < new_readyState)
				queued_readyState = new_readyState;
		}
		else
		{
			readyState = new_readyState;

			if (readyState == queued_readyState)
				queued_readyState = READYSTATE_UNINITIALIZED;

			/*  If there are any threads waiting for headers, unblock them now
			    as no headers will be received due to e.g. loading failed. */
			if (readyState == READYSTATE_LOADED)
				UnblockThreadsWaitingForHeaders();

			BOOL has_readystate_listeners = FALSE;
			if (DOM_EventTarget *event_target = GetEventTarget())
			{
				DOM_Runtime *runtime = GetRuntime();
				DOM_Event *event;
				if ((has_readystate_listeners = event_target->HasListeners(ONREADYSTATECHANGE, NULL, ES_PHASE_AT_TARGET)))
				{
					RETURN_IF_ERROR(DOMSetObjectRuntime(event = OP_NEW(DOM_Event, ()), runtime, runtime->GetPrototype(DOM_Runtime::EVENT_PROTOTYPE), "Event"));
					event->InitEvent(ONREADYSTATECHANGE, this);
					ES_Thread *event_thread = NULL;
					RETURN_IF_ERROR(GetEnvironment()->SendEvent(event, interrupt_thread, &event_thread));
					if (event_thread)
					{
						DOM_XHR_ReadyStateChangeListener *listener = OP_NEW(DOM_XHR_ReadyStateChangeListener, (this, interrupt_thread));
						RETURN_OOM_IF_NULL(listener);
						event_thread->AddListener(listener);
						readystatechanges_in_progress++;
					}
				}
				if (readyState == READYSTATE_LOADED && !error_flag && request_error_status == NoError)
				{
					/* Always send these two progress events when reaching the DONE/LOADED state. */
					RETURN_IF_ERROR(SendEvent(ONLOAD, interrupt_thread));
#ifdef PROGRESS_EVENTS_SUPPORT
					RETURN_IF_ERROR(SendProgressEvent(ONLOADEND, TRUE, FALSE, interrupt_thread));
					if (async)
						CancelProgressEvents();
#endif // PROGRESS_EVENTS_SUPPORT
					return OpStatus::OK;
				}
				else if (readyState == READYSTATE_LOADED && request_error_status != NoError)
					return SendRequestErrorEvent(interrupt_thread);
				else if (!has_readystate_listeners)
					/*  No listeners to wait for. Just step on. */
					AdvanceReadyStateTowardsActualReadyState(interrupt_thread);
			}
			else
				AdvanceReadyStateTowardsActualReadyState(interrupt_thread);
		}
	}

	return OpStatus::OK;
}

OP_STATUS
DOM_XMLHttpRequest::SetNetworkError(ES_Thread *interrupt_thread)
{
	if (request_error_status == NoError)
	{
		request_error_status = NetworkError;
		if (async)
			return SendRequestErrorEvent(interrupt_thread);
	}
	return OpStatus::OK;
}

OP_STATUS
DOM_XMLHttpRequest::SendRequestErrorEvent(ES_Thread *interrupt_thread)
{
	if (request_error_status != NoError && readyState == READYSTATE_LOADED)
	{
		DOM_EventType type = ONERROR;
		switch (request_error_status)
		{
		default:
			OP_ASSERT(!"Unhandled request error type; cannot happen");
			/* fallthrough */
		case NetworkError:
			type = ONERROR;
			break;
		case TimeoutError:
#ifdef PROGRESS_EVENTS_SUPPORT
			type = ONTIMEOUT;
#else
			type = ONERROR;
#endif // PROGRESS_EVENTS_SUPPORT
			break;
		case AbortError:
			type = ONABORT;
			break;
		}
#ifdef PROGRESS_EVENTS_SUPPORT
		RETURN_IF_ERROR(SendProgressEvent(type, TRUE, TRUE, interrupt_thread));
		RETURN_IF_ERROR(SendProgressEvent(ONLOADEND, TRUE, TRUE, interrupt_thread));
		CancelProgressEvents();
#else
		RETURN_IF_ERROR(SendEvent(type, interrupt_thread));
#endif // PROGRESS_EVENTS_SUPPORT
	}
	return OpStatus::OK;
}

int
DOM_XMLHttpRequest::ThrowRequestError(ES_Value *return_value)
{
	DOM_Object *this_object = this;
	switch (request_error_status)
	{
	default:
		OP_ASSERT(!"Error status match not complete; cannot happen.");
		/* fallthrough */
	case NetworkError:
		return DOM_CALL_DOMEXCEPTION(NETWORK_ERR);
	case AbortError:
		return DOM_CALL_DOMEXCEPTION(ABORT_ERR);
	case TimeoutError:
		return DOM_CALL_DOMEXCEPTION(TIMEOUT_ERR);
	}
}

OP_STATUS
DOM_XMLHttpRequest::AdvanceReadyStateToLoading(ES_Thread *interrupt_thread)
{
	if (GetActualReadyState() < DOM_XMLHttpRequest::READYSTATE_LOADING)
	{
		if (GetActualReadyState() < DOM_XMLHttpRequest::READYSTATE_HEADERS_RECEIVED)
			RETURN_IF_ERROR(SetReadyState(DOM_XMLHttpRequest::READYSTATE_HEADERS_RECEIVED, interrupt_thread));
		RETURN_IF_ERROR(SetReadyState(DOM_XMLHttpRequest::READYSTATE_LOADING, interrupt_thread));
	}
	return OpStatus::OK;
}

OP_STATUS
DOM_XMLHttpRequest::AddResponseData(const unsigned char *text, unsigned byte_length)
{
	if (byte_length == 0)
		return OpStatus::OK;

#ifdef PROGRESS_EVENTS_SUPPORT
	downloaded_bytes += static_cast<OpFileLength>(byte_length);
	RETURN_IF_ERROR(HandleProgressTick(FALSE));
#endif // PROGRESS_EVENTS_SUPPORT
	if (responseData.IsByteBuffer())
		return responseData.AppendBytes(text, byte_length);
	else
		return responseData.Append(reinterpret_cast<const uni_char *>(text), UNICODE_DOWNSIZE(byte_length));
}

void
DOM_XMLHttpRequest::SetResponseXML(DOM_Node *new_responseXML)
{
	responseXML = new_responseXML;
}

OP_STATUS
DOM_XMLHttpRequest::SetResponseHeaders(const uni_char *headers)
{
	return UniSetStr(responseHeaders, headers);
}

OP_STATUS
DOM_XMLHttpRequest::SetResponseHeaders(URL url)
{
	if (!responseHeaders)
	{
		OpString8 headers8;
		OpString headers;

		RETURN_IF_ERROR(url.GetAttribute(URL::KHTTPAllResponseHeadersL, headers8, TRUE));
		RETURN_IF_ERROR(headers.Set(headers8));
		RETURN_IF_ERROR(SetResponseHeaders(headers.CStr()));

		UnblockThreadsWaitingForHeaders();
	}

	return OpStatus::OK;
}

class ReopenXMLHttpRequestRestartObject
	: public DOM_Object
{
public:
	ES_Value abort_restart_object;
	ES_Value *args;
	unsigned argc;

	static OP_STATUS Make(ReopenXMLHttpRequestRestartObject*& restart_object, ES_Value abort_restart_object, ES_Value* argv, int argc, DOM_Runtime *runtime)
	{
		OP_ASSERT(argc >= 0);
		restart_object = OP_NEW(ReopenXMLHttpRequestRestartObject, ());
		RETURN_IF_ERROR(DOMSetObjectRuntime(restart_object, runtime, runtime->GetObjectPrototype(), "Object"));

		restart_object->abort_restart_object = abort_restart_object;
		restart_object->args = OP_NEWA(ES_Value, argc);
		RETURN_OOM_IF_NULL(restart_object->args);

		restart_object->argc = static_cast<unsigned>(argc);

		for (unsigned i = 0; i < restart_object->argc; i++)
			RETURN_IF_ERROR(DOMCopyValue(restart_object->args[i], argv[i]));

		return OpStatus::OK;
	}

	virtual void GCTrace()
	{
		GCMark(abort_restart_object);

		for (unsigned i = 0; i < argc; i++)
			GCMark(args[i]);
	}

private:
	ReopenXMLHttpRequestRestartObject()
		: args(NULL),
		  argc(0)
	{
	}

	virtual ~ReopenXMLHttpRequestRestartObject()
	{
		for (unsigned i = 0; i < argc; i++)
			DOMFreeValue(args[i]);

		OP_DELETEA(args);
	}
};

OP_STATUS
DOM_XMLHttpRequest::SendEvent(DOM_EventType event_type, ES_Thread *interrupt_thread, ES_Thread** created_thread /* = NULL */)
{
	if (created_thread)
		*created_thread = NULL;

	if (event_type == ONREADYSTATECHANGE)
	{
		DOM_Event* event;
		RETURN_IF_ERROR(DOMSetObjectRuntime(event = OP_NEW(DOM_Event, ()), GetRuntime(), GetRuntime()->GetPrototype(DOM_Runtime::EVENT_PROTOTYPE), "Event"));
		event->InitEvent(event_type, this);
		RETURN_IF_ERROR(GetEnvironment()->SendEvent(event, interrupt_thread, created_thread));
	}
	else
	{
#ifdef PROGRESS_EVENTS_SUPPORT
		OpFileLength bytes_total = 0;
		if (parser && parser->GetLoader())
			parser->GetLoader()->GetURL().GetAttribute(URL::KContentSize, &bytes_total, TRUE);

		DOM_ProgressEvent* progress_event;
		RETURN_IF_ERROR(DOM_ProgressEvent::Make(progress_event, GetRuntime()));
		switch (event_type)
		{
		default:
			OP_ASSERT(!"Unexpected progress event type; should not happen.");
			/* Fallthrough from 'impossible' case. */
		case ONTIMEOUT:
		case ONABORT:
		case ONERROR:
			progress_event->InitProgressEvent(bytes_total != 0, 0, bytes_total);
			break;
		case ONPROGRESS:
			last_downloaded_bytes = downloaded_bytes;
		case ONLOAD:
		case ONLOADSTART:
		case ONLOADEND:
			progress_event->InitProgressEvent(bytes_total != 0, downloaded_bytes, bytes_total);
			break;
		}
		progress_event->InitEvent(event_type, this);
		RETURN_IF_ERROR(GetEnvironment()->SendEvent(progress_event, interrupt_thread, created_thread));
#else
		DOM_Event* event;
		RETURN_IF_ERROR(DOMSetObjectRuntime(event = OP_NEW(DOM_Event, ()), GetRuntime(), GetRuntime()->GetPrototype(DOM_Runtime::EVENT_PROTOTYPE), "Event"));
		event->InitEvent(event_type, this);
		RETURN_IF_ERROR(GetEnvironment()->SendEvent(event, interrupt_thread, created_thread));
#endif // PROGRESS_EVENTS_SUPPORT
	}

	return OpStatus::OK;
}

#ifdef PROGRESS_EVENTS_SUPPORT
OP_STATUS
DOM_XMLHttpRequest::SendProgressEvent(DOM_EventType event_type, BOOL also_as_upload, BOOL download_last, ES_Thread *interrupt_thread)
{
	if (!download_last && (event_type != ONPROGRESS || downloaded_bytes != last_downloaded_bytes))
		RETURN_IF_ERROR(SendEvent(event_type, interrupt_thread));

	if (also_as_upload && uploader && !upload_complete_flag)
	{
		OpFileLength bytes_total = 0;
		if (request->GetOutputStream())
			bytes_total = request->GetOutputStream()->GetContentLength();
		OpFileLength length = uploader->GetUploadedBytes();
		if (event_type != ONPROGRESS || length != uploader->GetPreviousUploadedBytes())
		{
			uploader->SetPreviousUploadedBytes(length);
			RETURN_IF_ERROR(uploader->SendEvent(event_type, length, bytes_total, interrupt_thread));
		}
	}
	if (download_last && (event_type != ONPROGRESS || downloaded_bytes != last_downloaded_bytes))
		RETURN_IF_ERROR(SendEvent(event_type, interrupt_thread));

	return OpStatus::OK;
}

OP_STATUS
DOM_XMLHttpRequest::StartProgressEvents(ES_Runtime *origining_runtime)
{
	if (!progress_events_flag && async)
	{
		progress_events_flag = TRUE;

		RETURN_IF_ERROR(SendProgressEvent(ONLOADSTART, TRUE, FALSE, GetCurrentThread(origining_runtime)));

		last_progress_event_time = 0.0;
		delayed_progress_event_pending = FALSE;
	}

	return OpStatus::OK;
}

void
DOM_XMLHttpRequest::CancelProgressEvents()
{
	progress_events_flag = FALSE;
	upload_complete_flag = TRUE;

	last_progress_event_time = 0.0;
	delayed_progress_event_pending = FALSE;
}

OP_BOOLEAN
DOM_XMLHttpRequest::HandleTimeout()
{
	if (GetActualReadyState() != READYSTATE_DONE)
	{
		error_flag = TRUE;
		request_error_status = TimeoutError;
		if (async)
			RETURN_IF_ERROR(SendRequestErrorEvent(NULL));

		return OpBoolean::IS_TRUE;
	}
	else
		return OpBoolean::IS_FALSE;
}

unsigned
DOM_XMLHttpRequest::GetTimeoutMS()
{
	return timeout_value;
}

OP_STATUS
DOM_XMLHttpRequest::HandleProgressTick(BOOL also_upload_progress)
{
	double current_time = g_op_time_info->GetRuntimeMS();
	unsigned elapsed_time = static_cast<unsigned>(current_time - last_progress_event_time);
	if (elapsed_time >= DOM_XHR_PROGRESS_EVENT_INTERVAL)
	{
		RETURN_IF_ERROR(SendProgressEvent(ONPROGRESS, TRUE, FALSE, NULL));
		last_progress_event_time = current_time;
	}
	else if (!delayed_progress_event_pending && parser && parser->GetLoader())
	{
		RETURN_IF_ERROR(parser->GetLoader()->PostProgressTick(DOM_XHR_PROGRESS_EVENT_INTERVAL - elapsed_time));
		delayed_progress_event_pending = TRUE;
	}

	return OpStatus::OK;
}

OP_STATUS
DOM_XMLHttpRequest::HandleUploadProgress(OpFileLength delta)
{
	if (!uploader || upload_complete_flag)
		return OpStatus::OK;

	if (delta == 0)
		return FinishedUploading();

	unsigned bytes = TruncateDoubleToUInt(static_cast<double>(delta));
	double current_time = g_op_time_info->GetRuntimeMS();
	unsigned elapsed_time = static_cast<unsigned>(current_time - last_progress_event_time);
	if (elapsed_time >= DOM_XHR_PROGRESS_EVENT_INTERVAL)
	{
		uploader->AddUploadedBytes(bytes, FALSE);
		OpFileLength bytes_total = 0;
		if (request->GetOutputStream())
			bytes_total = request->GetOutputStream()->GetContentLength();

		OpFileLength length = uploader->GetUploadedBytes();
		if (length != uploader->GetPreviousUploadedBytes())
		{
			uploader->SetPreviousUploadedBytes(length);
			RETURN_IF_ERROR(uploader->SendEvent(ONPROGRESS, length, bytes_total));
		}
		last_progress_event_time = current_time;
	}
	else if (!delayed_progress_event_pending && parser && parser->GetLoader())
	{
		uploader->AddUploadedBytes(bytes, TRUE);
		RETURN_IF_ERROR(parser->GetLoader()->PostProgressTick(DOM_XHR_PROGRESS_EVENT_INTERVAL - elapsed_time));
		delayed_progress_event_pending = TRUE;
	}

	return OpStatus::OK;
}
#endif // PROGRESS_EVENTS_SUPPORT

BOOL
DOM_XMLHttpRequest::HasEventHandlerAndUnsentEvents()
{
	if (readystatechanges_in_progress > 0)
		return TRUE;
	if (queued_readyState != READYSTATE_UNSENT)
	{
		if (DOM_EventTarget *event_target = GetEventTarget())
		{
#ifdef PROGRESS_EVENTS_SUPPORT
			const DOM_EventType event_types[] = {ONREADYSTATECHANGE, ONLOAD, ONERROR, ONABORT, ONTIMEOUT, ONLOADSTART, ONLOADEND, ONPROGRESS};
#else
			const DOM_EventType event_types[] = {ONREADYSTATECHANGE, ONLOAD, ONERROR};
#endif // PROGRESS_EVENTS_SUPPORT

			for (unsigned i = 0; i < ARRAY_SIZE(event_types); i++)
				if (event_target->HasListeners(event_types[i], NULL, ES_PHASE_AT_TARGET))
					return TRUE;
		}
#ifdef PROGRESS_EVENTS_SUPPORT
		if (!upload_complete_flag)
			return HaveUploadEventListeners();
#endif // PROGRESS_EVENTS_SUPPORT
	}

	return FALSE;
}

/* static */ int
DOM_XMLHttpRequest::open(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(xmlhttprequest, DOM_TYPE_XMLHTTPREQUEST, DOM_XMLHttpRequest);

	if (argc >= 0)
	{
		DOM_CHECK_ARGUMENTS("ss");

		if (xmlhttprequest->GetActualReadyState() == READYSTATE_OPENED && xmlhttprequest->send_flag ||
		    xmlhttprequest->GetActualReadyState() == READYSTATE_HEADERS_RECEIVED ||
		    xmlhttprequest->GetActualReadyState() == READYSTATE_RECEIVING)
		{
			ES_Value abort_return_value;

			int result = DOM_XMLHttpRequest::abort(xmlhttprequest, NULL, 0, &abort_return_value, origining_runtime);

			if (result == (ES_SUSPEND | ES_RESTART))
			{
				ReopenXMLHttpRequestRestartObject *restart_object;

				CALL_FAILED_IF_ERROR(ReopenXMLHttpRequestRestartObject::Make(restart_object, abort_return_value, argv, argc, origining_runtime));
				DOMSetObject(return_value, restart_object);
				return result;
			}
		}
	}
	else
	{
		ReopenXMLHttpRequestRestartObject *restart_object = static_cast<ReopenXMLHttpRequestRestartObject*>(DOM_GetHostObject(return_value->value.object));

		int result = DOM_XMLHttpRequest::abort(xmlhttprequest, NULL, -1, &restart_object->abort_restart_object, origining_runtime);
		if (result == (ES_SUSPEND | ES_RESTART))
		{
			/* We're suspending yet again. The restart object needs to be kept as is since it has
			   all the restart data needed by both this function and the abort(). */
			return result;
		}

		argv = restart_object->args;
		argc = static_cast<int>(restart_object->argc);
	}

	xmlhttprequest->async = TRUE;
	if (argc > 2 && argv[2].type == VALUE_BOOLEAN)
		xmlhttprequest->async = argv[2].value.boolean;

	const uni_char *method = argv[0].value.string;

	if (!DOM_Utils::IsValidTokenValue(method, argv[0].GetStringLength()))
		return DOM_CALL_DOMEXCEPTION(SYNTAX_ERR);

	const uni_char *method_canonical = NormaliseLegalMethod(method);
	if (!method_canonical)
		return DOM_CALL_DOMEXCEPTION(SECURITY_ERR);

	// Check some basic properties of the URL and throw appropriate exceptions.
	URL current_url = xmlhttprequest->GetRuntime()->GetOriginURL();
	URL target_url = g_url_api->GetURL(current_url, argv[1].value.string);
	if (target_url.Type() == URL_NULL_TYPE)
		return DOM_CALL_DOMEXCEPTION(SYNTAX_ERR);
	if (target_url.Type() == URL_UNKNOWN)
		return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);

	if (argc > 4 && argv[3].type == VALUE_STRING && argv[4].type == VALUE_STRING)
	{
		CALL_FAILED_IF_ERROR(UniSetStr(xmlhttprequest->username, argv[3].value.string));
		CALL_FAILED_IF_ERROR(UniSetStr(xmlhttprequest->password, argv[4].value.string));
	}

#ifdef DOM_XMLHTTPREQUEST_CONSTRAIN_SYNC_USAGE
	if (xmlhttprequest->HasWindowDocument() &&
	    (xmlhttprequest->timeout_value > 0 ||
# ifdef CORS_SUPPORT
		xmlhttprequest->with_credentials ||
# endif // CORS_SUPPORT
		xmlhttprequest->responseType && *xmlhttprequest->responseType))
		return DOM_CALL_DOMEXCEPTION(INVALID_STATE_ERR);
#endif // DOM_XMLHTTPREQUEST_CONSTRAIN_SYNC_USAGE

	DOM_EnvironmentImpl *environment = xmlhttprequest->GetEnvironment();
	CALL_FAILED_IF_ERROR(DOM_HTTPRequest::Make(xmlhttprequest->request, environment, argv[1].value.string, method_canonical));
	CALL_FAILED_IF_ERROR(xmlhttprequest->SetReadyState(READYSTATE_OPEN, GetCurrentThread(origining_runtime), TRUE));

	CALL_FAILED_IF_ERROR(DOM_LSParser::Make(xmlhttprequest->parser, environment, xmlhttprequest->async, xmlhttprequest));
	xmlhttprequest->parser->SetBaseURL();

#ifdef CORS_SUPPORT
	CALL_FAILED_IF_ERROR(xmlhttprequest->CheckIfCrossOriginCandidate(target_url));
#endif // CORS_SUPPORT

	xmlhttprequest->responseData.Clear();
	xmlhttprequest->responseXML = NULL;
	xmlhttprequest->responseBuffer = NULL;
	xmlhttprequest->responseBlob = NULL;

	if (xmlhttprequest->responseType)
	{
		BOOL is_binary_type = FALSE;
		if (IsSupportedResponseType(xmlhttprequest->responseType, is_binary_type))
		{
			xmlhttprequest->parser->SetParseLoadedData(!is_binary_type);
			xmlhttprequest->responseData.SetIsByteBuffer(is_binary_type);
		}
	}

	OP_DELETEA(xmlhttprequest->responseHeaders);
	xmlhttprequest->responseHeaders = NULL;

	xmlhttprequest->send_flag = FALSE;
	xmlhttprequest->request_error_status = NoError;
#ifdef PROGRESS_EVENTS_SUPPORT
	xmlhttprequest->CancelProgressEvents();
#endif // PROGRESS_EVENTS_SUPPORT

	if (!xmlhttprequest->forced_content_type.IsEmpty())
		CALL_FAILED_IF_ERROR(xmlhttprequest->request->ForceContentType(xmlhttprequest->forced_content_type.CStr()));

	return ES_FAILED;
}

#ifdef CORS_SUPPORT
OP_STATUS
DOM_XMLHttpRequest::CreateCrossOriginRequest(const URL &origin_url, const URL &target_url)
{
	OP_ASSERT(want_cors_request);
	OP_ASSERT(!cross_origin_request);

	const uni_char *method = GetMethod();
	BOOL with_credentials = IsCredentialled();
	RETURN_IF_ERROR(OpCrossOrigin_Request::Make(cross_origin_request, origin_url, target_url, method, with_credentials, is_anonymous));
#ifdef PROGRESS_EVENTS_SUPPORT
	if (async && HaveUploadEventListeners())
		cross_origin_request->SetPreflightRequired();
#endif // PROGRESS_EVENTS_SUPPORT

	DOM_HTTPRequest::Header *header = request->GetFirstHeader();
	while (header)
	{
		RETURN_IF_ERROR(cross_origin_request->AddHeader(header->name.CStr(), header->value.CStr()));
		header = header->next;
	}
	if (DOM_HTTPOutputStream *outputstream = request->GetOutputStream())
		if (outputstream->GetContentType())
			RETURN_IF_ERROR(cross_origin_request->AddHeader("Content-Type", outputstream->GetContentType()));

	return OpStatus::OK;
}

OP_STATUS
DOM_XMLHttpRequest::SetCrossOriginRequest(OpCrossOrigin_Request *cors_request)
{
	cross_origin_request = cors_request;
#ifdef PROGRESS_EVENTS_SUPPORT
	if (async && HaveUploadEventListeners())
		cross_origin_request->SetPreflightRequired();
#endif // PROGRESS_EVENTS_SUPPORT

	DOM_HTTPRequest::Header *header = request->GetFirstHeader();
	while (header)
	{
		RETURN_IF_ERROR(cross_origin_request->AddHeader(header->name.CStr(), header->value.CStr()));
		header = header->next;
	}
	if (DOM_HTTPOutputStream *outputstream = request->GetOutputStream())
		if (outputstream->GetContentType())
			RETURN_IF_ERROR(cross_origin_request->AddHeader("Content-Type", outputstream->GetContentType()));

	return OpStatus::OK;
}

void
DOM_XMLHttpRequest::ResetCrossOrigin()
{
	OP_DELETE(cross_origin_request);
	cross_origin_request = NULL;
	want_cors_request = is_cors_request = FALSE;
}

OP_BOOLEAN
DOM_XMLHttpRequest::CheckIfCrossOriginCandidate(const URL &target_url)
{
	/* Check if a cross-origin candidate and mark the object if so.
	   This is needed so that configuration of the XHR object happens
	   using CORS rules. The actual security check happens upon send();
	   CORS_PRE_ACCESS_CHECK is a (synchronous) check if it qualifies
	   for cross-origin consideration. */
	ResetCrossOrigin();
	OpSecurityContext source_context(GetRuntime());
	OpSecurityContext target_context(target_url);
	RETURN_IF_ERROR(OpSecurityManager::CheckSecurity(OpSecurityManager::CORS_PRE_ACCESS_CHECK, source_context, target_context, want_cors_request));
	return want_cors_request ? OpBoolean::IS_TRUE : OpBoolean::IS_FALSE;
}

OP_STATUS
DOM_XMLHttpRequest::UpdateCrossOriginStatus(URL &url, BOOL allowed, BOOL &is_security_error)
{
	if (!want_cors_request)
		return OpStatus::OK;

	if (cross_origin_request && cross_origin_request->IsNetworkError())
	{
		is_security_error = FALSE;
		return SetNetworkError(NULL);
	}

	if (allowed && cross_origin_request && !cross_origin_request->IsActive())
	{
		/* Not going ahead as a CORS request; disable the
		   CORS flags and release the request. */
		want_cors_request = FALSE;
		OP_DELETE(cross_origin_request);
		cross_origin_request = NULL;
		allowed = FALSE;
	}
	is_cors_request = allowed;

	if (!want_cors_request)
		RETURN_IF_ERROR(SetCredentials(url));

	return OpStatus::OK;
}
#endif // CORS_SUPPORT

int
DOM_XMLHttpRequest::PrepareUploadData(ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	const unsigned char *byte_data = NULL;
	unsigned byte_length = 0;
	BOOL is_binary = FALSE;
	DOM_Object *external_object = NULL;
	BOOL matched = FALSE;
	const uni_char *preferred_mime_type = NULL;
	const char *preferred_encoding = NULL;

	ES_Value *arg = NULL;

	if (argc < 0)
	{
		OP_ASSERT(blocked_on_prepare_upload);
		DOM_Object *reader = static_cast<DOM_Object *>(ES_Runtime::GetHostObject(return_value->value.object));
		OP_ASSERT(reader && reader->IsA(DOM_TYPE_FILEREADER));
		int result = DOM_FileReader::readBlob(reader, NULL, byte_data, byte_length, return_value, origining_runtime);
		if (result == ES_VALUE)
		{
			blocked_on_prepare_upload = FALSE;
			OP_ASSERT(byte_data);
			arg = return_value;
			argc = 1;
		}
		else
			return result;
	}

	if (argc > 0)
	{
		if (!arg)
			arg = &argv[0];
		if (arg->type == VALUE_STRING_WITH_LENGTH)
		{
			byte_data = reinterpret_cast<const unsigned char *>(arg->value.string_with_length->string);
			byte_length = UNICODE_SIZE(arg->value.string_with_length->length);
			preferred_mime_type = UNI_L("text/plain;charset=UTF-8");
			preferred_encoding = "UTF-8";
			matched = TRUE;
		}
		else if (arg->type == VALUE_OBJECT || byte_data)
		{
			if (byte_data)
			{
				is_binary = TRUE;
				matched = TRUE;
			}
			else if (ES_Runtime::IsNativeArrayBufferObject(arg->value.object))
			{
				byte_data = ES_Runtime::GetArrayBufferStorage(argv[0].value.object);
				byte_length = ES_Runtime::GetArrayBufferLength(argv[0].value.object);
				is_binary = TRUE;
				matched = TRUE;
			}
			else if (ES_Runtime::IsNativeTypedArrayObject(arg->value.object))
			{
				byte_data = reinterpret_cast<unsigned char *>(ES_Runtime::GetStaticTypedArrayStorage(argv[0].value.object));
				byte_length = ES_Runtime::GetStaticTypedArraySize(argv[0].value.object);
				is_binary = TRUE;
				matched = TRUE;
			}
			else if (DOM_Object *host_object = DOM_HOSTOBJECT(arg->value.object, DOM_Object))
			{
				if (host_object->IsA(DOM_TYPE_FORMDATA) || host_object->IsA(DOM_TYPE_FILE))
				{
					external_object = host_object;
					if (host_object->IsA(DOM_TYPE_FILE))
						preferred_mime_type = static_cast<DOM_File *>(host_object)->GetContentType();
					matched = TRUE;
				}
				else if (host_object->IsA(DOM_TYPE_BLOB))
				{
					DOM_Object *reader;
					int result = DOM_FileReader::readBlob(reader, static_cast<DOM_Blob *>(host_object), byte_data, byte_length, return_value, origining_runtime);
					if (result & ES_SUSPEND)
					{
						DOMSetObject(return_value, reader);
						this->blocked_on_prepare_upload = TRUE;
						return result;
					}
					else if (result != ES_VALUE)
						return result;
					else
					{
						OP_ASSERT(byte_data || byte_length == 0);
						is_binary = TRUE;
						matched = TRUE;
					}
				}
				else if (host_object->IsA(DOM_TYPE_NODE))
				{
					DOM_Node *node = static_cast<DOM_Node *>(host_object);

					DOM_LSSerializer *lsserializer;
					CALL_FAILED_IF_ERROR(DOM_LSSerializer::Make(lsserializer, GetEnvironment()));

					ES_Value write_arg, write_return_value;
					DOMSetObject(&write_arg, node);

					int result = DOM_LSSerializer::write(lsserializer, &write_arg, 1, &write_return_value, origining_runtime, 2);

					/* Shouldn't happen if the serializer has no filter, which it hasn't. */
					OP_ASSERT(result != (ES_SUSPEND | ES_RESTART));

					if (result != ES_VALUE)
						return result;

					OP_ASSERT(write_return_value.type == VALUE_STRING);

					byte_data = reinterpret_cast<const unsigned char *>(write_return_value.value.string);
					byte_length = UNICODE_SIZE(uni_strlen(write_return_value.value.string));
					BOOL is_xml = node->GetOwnerDocument() && node->GetOwnerDocument()->IsXML();
					preferred_mime_type = is_xml ? UNI_L("application/xml;charset=UTF-8") : UNI_L("text/html;charset=UTF-8");
					preferred_encoding = "UTF-8";
					matched = TRUE;
				}
			}
		}
		if (!matched && arg->type != VALUE_NULL && arg->type != VALUE_UNDEFINED)
		{
			DOMSetNumber(return_value, ES_CONVERT_ARGUMENT(ES_CALL_NEEDS_STRING_WITH_LENGTH, 0));
			return ES_NEEDS_CONVERSION;
		}
	}

	DOM_HTTPOutputStream *output_stream = request->GetOutputStream();

	CALL_FAILED_IF_ERROR(output_stream->SetData(byte_data, byte_length, is_binary, external_object));
	if (preferred_mime_type || preferred_encoding)
		CALL_FAILED_IF_ERROR(output_stream->UpdateContentType(preferred_mime_type, preferred_encoding));
#ifdef PROGRESS_EVENTS_SUPPORT
	if ((external_object || byte_length > 0) && async)
		upload_complete_flag = FALSE;
#endif // PROGRESS_EVENTS_SUPPORT
	return ES_FAILED;
}


#ifdef URL_FILTER
/* static */ OP_STATUS
DOM_XMLHttpRequest::CheckContentFilter(DOM_XMLHttpRequest *xmlhttprequest, DOM_Runtime* origining_runtime, BOOL &allowed)
{
	OpString display_url_str;

	RETURN_IF_ERROR(origining_runtime->GetDisplayURL(display_url_str));
	URL display_url = g_url_api->GetURL(display_url_str);
	ServerName *sn = display_url.GetServerName();
	FramesDocument *doc = origining_runtime->GetEnvironment()->GetFramesDocument();
	BOOL widget = doc && doc->GetWindow()->GetType() == WIN_TYPE_GADGET;

	// No HTML_Element for xhr should be fine
	URLFilterDOMListenerOverride lsn_over(origining_runtime->GetEnvironment(), NULL);
	HTMLLoadContext load_ctx(RESOURCE_XMLHTTPREQUEST, sn, &lsn_over, widget);
	URL url = urlManager->GetURL(xmlhttprequest->request->GetURI());

	return g_urlfilter->CheckURL(&url, allowed, NULL,  &load_ctx);
}
#endif // URL_FILTER

/* static */ int
DOM_XMLHttpRequest::send(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(xmlhttprequest, DOM_TYPE_XMLHTTPREQUEST, DOM_XMLHttpRequest);

	BOOL have_completed_upload = FALSE;

	if (argc < 0 && xmlhttprequest->blocked_on_prepare_upload)
	{
		int result = xmlhttprequest->PrepareUploadData(argv, argc, return_value, origining_runtime);
		if (result != ES_FAILED)
			return result;

		OP_ASSERT(!xmlhttprequest->blocked_on_prepare_upload);
		have_completed_upload = TRUE;
		argc = 0;
	}

	if (argc >= 0
#ifdef PROGRESS_EVENTS_SUPPORT
		|| xmlhttprequest->delivered_onloadstart
#endif // PROGRESS_EVENTS_SUPPORT
		)
	{
		if (xmlhttprequest->GetActualReadyState() != READYSTATE_OPEN)
			return DOM_CALL_DOMEXCEPTION(INVALID_STATE_ERR);

#ifdef URL_FILTER
		BOOL allowed = FALSE;

		CALL_FAILED_IF_ERROR(CheckContentFilter(xmlhttprequest, origining_runtime, allowed));

		if (!allowed)
			return ES_FAILED;
#endif // URL_FILTER

		xmlhttprequest->headers_received = FALSE;
#ifdef PROGRESS_EVENTS_SUPPORT
		if (!xmlhttprequest->delivered_onloadstart)
			xmlhttprequest->upload_complete_flag = TRUE;
#endif // PROGRESS_EVENTS_SUPPORT

		BOOL has_upload_data = IsUploadMethod(xmlhttprequest->request->GetMethod());
#ifdef PROGRESS_EVENTS_SUPPORT
		if (xmlhttprequest->delivered_onloadstart)
			has_upload_data = FALSE;
#endif // PROGRESS_EVENTS_SUPPORT

		if (has_upload_data && !have_completed_upload)
		{
			int result = xmlhttprequest->PrepareUploadData(argv, argc, return_value, origining_runtime);
			if (result != ES_FAILED)
				return result;
		}

		xmlhttprequest->error_flag = FALSE;

		if (xmlhttprequest->async)
		{
			xmlhttprequest->send_flag = TRUE;

#ifdef PROGRESS_EVENTS_SUPPORT
			if (!xmlhttprequest->delivered_onloadstart)
			{
				CALL_FAILED_IF_ERROR(xmlhttprequest->StartProgressEvents(origining_runtime));
				if (GetCurrentThread(origining_runtime)->IsBlocked())
				{
					xmlhttprequest->delivered_onloadstart = TRUE;
					return ES_SUSPEND | ES_RESTART;
				}
			}
			else
				xmlhttprequest->delivered_onloadstart = FALSE;
#endif // PROGRESS_EVENTS_SUPPORT
		}

		ES_Value parse_argv[1];
		DOMSetObject(&parse_argv[0], xmlhttprequest->GetRequest()->GetInput());

		int result = DOM_LSParser::parse(xmlhttprequest->parser, parse_argv, 1, return_value, origining_runtime, 0);

		if (result == ES_VALUE || result == (ES_SUSPEND | ES_RESTART))
		{
			if (xmlhttprequest->request_error_status != NoError)
				return xmlhttprequest->ThrowRequestError(return_value);
		}
		else if (result == (ES_SUSPEND | ES_ABORT))
			result = (ES_SUSPEND | ES_RESTART);
		else
		{
			xmlhttprequest->error_flag = TRUE;

			/* Error case. Advance state to "loaded" and forget about this request. */
			CALL_FAILED_IF_ERROR(xmlhttprequest->SetReadyState(READYSTATE_LOADED, GetCurrentThread(origining_runtime), TRUE));
			if (xmlhttprequest->request_error_status != NoError)
				return xmlhttprequest->ThrowRequestError(return_value);
		}

		return result;
	}
	else
	{
		int result = DOM_LSParser::parse(xmlhttprequest->parser, NULL, -1, return_value, origining_runtime, 0);

		if (result == ES_VALUE || result == (ES_SUSPEND | ES_RESTART))
		{
			xmlhttprequest->error_flag = FALSE;

			if (xmlhttprequest->async)
				xmlhttprequest->send_flag = TRUE;
		}
		else if (result == (ES_SUSPEND | ES_ABORT))
			result = (ES_SUSPEND | ES_RESTART);
		else
		{
			xmlhttprequest->error_flag = TRUE;
			CALL_FAILED_IF_ERROR(xmlhttprequest->SetReadyState(READYSTATE_LOADED, GetCurrentThread(origining_runtime), TRUE));
			if (!xmlhttprequest->async && xmlhttprequest->request_error_status != NoError)
				return xmlhttprequest->ThrowRequestError(return_value);
		}

		return result;
	}
}

/* static */ int
DOM_XMLHttpRequest::abort(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_XMLHttpRequest *xmlhttprequest;

	if (argc >= 0)
	{
		DOM_THIS_OBJECT_EXISTING(xmlhttprequest, DOM_TYPE_XMLHTTPREQUEST, DOM_XMLHttpRequest);

		/* Can't fail. */
		DOM_LSParser::abort(xmlhttprequest->parser, argv, 0, return_value, origining_runtime);

		xmlhttprequest->error_flag = TRUE;

		xmlhttprequest->queued_readyState = READYSTATE_UNSENT;

		switch (xmlhttprequest->readyState)
		{
		case READYSTATE_OPENED:
			if (!xmlhttprequest->send_flag)
				break;

		case READYSTATE_HEADERS_RECEIVED:
		case READYSTATE_LOADING:
			ES_Thread *current_thread = GetCurrentThread(origining_runtime);
			xmlhttprequest->send_flag = FALSE;
			CALL_FAILED_IF_ERROR(xmlhttprequest->SetReadyState(READYSTATE_DONE, current_thread, TRUE));
#ifdef PROGRESS_EVENTS_SUPPORT
			CALL_FAILED_IF_ERROR(xmlhttprequest->SendProgressEvent(ONABORT, FALSE, FALSE, current_thread));
			CALL_FAILED_IF_ERROR(xmlhttprequest->SendProgressEvent(ONLOADEND, FALSE, FALSE, current_thread));
			if (xmlhttprequest->uploader && !xmlhttprequest->upload_complete_flag)
			{
				OpFileLength bytes_total = 0;
				if (xmlhttprequest->request->GetOutputStream())
					bytes_total = xmlhttprequest->request->GetOutputStream()->GetContentLength();
				OpFileLength length = xmlhttprequest->uploader->GetUploadedBytes();
				/* The delivery isn't paired over event types in the spec (but direction), hence
				   the ordering. */
				CALL_FAILED_IF_ERROR(xmlhttprequest->uploader->SendEvent(ONABORT, length, bytes_total, current_thread));
				CALL_FAILED_IF_ERROR(xmlhttprequest->uploader->SendEvent(ONLOADEND, length, bytes_total, current_thread));
			}
			xmlhttprequest->CancelProgressEvents();
#endif // PROGRESS_EVENTS_SUPPORT

			if (current_thread->IsBlocked())
			{
				DOMSetObject(return_value, this_object);
				return ES_SUSPEND | ES_RESTART;
			}
		}
#ifdef PROGRESS_EVENTS_SUPPORT
		xmlhttprequest->CancelProgressEvents();
#endif // PROGRESS_EVENTS_SUPPORT
	}
	else
		xmlhttprequest = DOM_VALUE2OBJECT(*return_value, DOM_XMLHttpRequest);

	xmlhttprequest->readyState = xmlhttprequest->queued_readyState = READYSTATE_UNSENT;
	return ES_FAILED;
}

#ifdef CORS_SUPPORT
/* static */ OP_STATUS
DOM_XMLHttpRequest::ProcessCrossOriginResponseHeaders(OpCrossOrigin_Request *request, TempBuffer *buffer, const uni_char *headers)
{
	BOOL first = TRUE;
	while (*headers)
	{
		const uni_char *header_start = headers;

		while (*headers && *headers != ':' && *headers != '\r' && *headers != '\n')
			++headers;

		if (*headers == ':' && request->CanExposeResponseHeader(header_start, static_cast<unsigned>(headers - header_start)))
		{
			const uni_char *value = headers + 1;
			while (*value == ' ')
				++value;

			while (*value && *value != '\r' && *value != '\n')
				++value;

			if (!first)
				RETURN_IF_ERROR(buffer->Append("\r\n"));

			RETURN_IF_ERROR(buffer->Append(header_start, static_cast<unsigned>(value - header_start)));
			headers = *value ? value + 1 : value;
		}

		while (*headers && *headers != '\r' && *headers != '\n')
			++headers;

		while (*headers && (*headers == '\r' || *headers == '\n'))
			++headers;

		first = FALSE;
	}
	return OpStatus::OK;
}
#endif // CORS_SUPPORT

/* static */ int
DOM_XMLHttpRequest::getAllResponseHeaders(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(xmlhttprequest, DOM_TYPE_XMLHTTPREQUEST, DOM_XMLHttpRequest);

	if (argc >= 0)
	{
		if (xmlhttprequest->error_flag || xmlhttprequest->readyState < READYSTATE_HEADERS_RECEIVED)
		{
			DOMSetString(return_value);
			return ES_VALUE;
		}

		if (!xmlhttprequest->headers_received)
		{
			ES_Thread* curr_thread = GetCurrentThread(origining_runtime);
			curr_thread->AddListener(&xmlhttprequest->waiting_for_headers_thread_listener);
			CALL_FAILED_IF_ERROR(xmlhttprequest->threads_waiting_for_headers.Add(curr_thread));
			curr_thread->Block();
			return ES_SUSPEND | ES_RESTART;
		}
	}

#ifdef CORS_SUPPORT
	if (OpCrossOrigin_Request *request = xmlhttprequest->GetCrossOriginRequest())
	{
		BOOL allowed = FALSE;
		if (request->HasCompleted(allowed) && allowed)
		{
			TempBuffer *buffer = GetEmptyTempBuf();
			CALL_FAILED_IF_ERROR(ProcessCrossOriginResponseHeaders(request, buffer, xmlhttprequest->responseHeaders));
			DOMSetString(return_value, buffer);
		}
		else
			DOMSetString(return_value);
	}
	else
#endif // CORS_SUPPORT
		DOMSetString(return_value, xmlhttprequest->responseHeaders);

	return ES_VALUE;
}

/* static */ int
DOM_XMLHttpRequest::getResponseHeader(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(xmlhttprequest, DOM_TYPE_XMLHTTPREQUEST, DOM_XMLHttpRequest);

	const uni_char *value = NULL;
	const uni_char *header;

	if (argc >= 0)
	{
		DOM_CHECK_ARGUMENTS("s");

		if (xmlhttprequest->error_flag || xmlhttprequest->readyState < READYSTATE_HEADERS_RECEIVED)
		{
			DOMSetNull(return_value);
			return ES_VALUE;
		}

		header = argv[0].value.string;
	}
	else
	{
		OP_ASSERT(return_value->type == VALUE_STRING);
		header = return_value->value.string;
	}

	const uni_char *headers = xmlhttprequest->responseHeaders;
	unsigned header_length = uni_strlen(header);

	if (headers)
	{
#ifdef CORS_SUPPORT
		if (OpCrossOrigin_Request *request = xmlhttprequest->GetCrossOriginRequest())
		{
			BOOL allowed = FALSE;
			if (request->HasCompleted(allowed) && allowed)
				if (!request->CanExposeResponseHeader(header))
				{
					DOMSetNull(return_value);
					return ES_VALUE;
				}
		}
#endif // CORS_SUPPORT

		while (*headers)
		{
			const uni_char *header_start = headers;

			while (*headers && *headers != ':' && *headers != '\r' && *headers != '\n')
				++headers;

			if (*headers == ':')
			{
				if ((unsigned) (headers - header_start) == header_length && uni_strnicmp(header_start, header, header_length) == 0)
				{
					value = headers + 1;

					while (*value == ' ')
						++value;

					const uni_char *value_end = value;

					while (*value_end && *value_end != '\r' && *value_end != '\n')
						++value_end;

					TempBuffer *buffer = GetEmptyTempBuf();
					CALL_FAILED_IF_ERROR(buffer->Append(value, value_end - value));
					value = buffer->GetStorage();

					break;
				}

				while (*headers && *headers != '\r' && *headers != '\n')
					++headers;
			}

			while (*headers && (*headers == '\r' || *headers == '\n'))
				++headers;
		}
	}
	else
		if (argc >= 0 && !xmlhttprequest->headers_received)
		{
			ES_Thread* curr_thread = GetCurrentThread(origining_runtime);
			curr_thread->AddListener(&xmlhttprequest->waiting_for_headers_thread_listener);
			CALL_FAILED_IF_ERROR(xmlhttprequest->threads_waiting_for_headers.Add(curr_thread));
			curr_thread->Block();
			TempBuffer* buff = GetEmptyTempBuf();
			CALL_FAILED_IF_ERROR(buff->Append(header));
			DOMSetString(return_value, buff);
			return ES_SUSPEND | ES_RESTART;
		}

	if (value)
		DOMSetString(return_value, value);
	else
		DOMSetNull(return_value);

	return ES_VALUE;
}

/* static */ int
DOM_XMLHttpRequest::setRequestHeader(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(xmlhttprequest, DOM_TYPE_XMLHTTPREQUEST, DOM_XMLHttpRequest);
	DOM_CHECK_ARGUMENTS("ss");

	if (xmlhttprequest->GetActualReadyState() != READYSTATE_OPEN || xmlhttprequest->send_flag)
		return DOM_CALL_DOMEXCEPTION(INVALID_STATE_ERR);

	if (!DOM_Utils::IsValidTokenValue(argv[0].value.string, argv[0].GetStringLength()))
		return DOM_CALL_DOMEXCEPTION(SYNTAX_ERR);

	const uni_char *field_value = argv[1].value.string;
	while (*field_value)
		if (*field_value++ > 0xff)
			return DOM_CALL_DOMEXCEPTION(SYNTAX_ERR);

	CALL_FAILED_IF_ERROR(xmlhttprequest->request->AddHeader(argv[0].value.string, argv[1].value.string));

	return ES_FAILED;
}

/* static */ int
DOM_XMLHttpRequest::overrideMimeType(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(xmlhttprequest, DOM_TYPE_XMLHTTPREQUEST, DOM_XMLHttpRequest);
	DOM_CHECK_ARGUMENTS("s");

	/* We are allowed to call overrideMimeType() before calling open(),
	 * so we need to remember it if we aren't in the open state.
	 * [Actually we are not according to XHR2 as of 2011-11 :), but an extension
	 *  that might be added soon.]
	 */
	CALL_FAILED_IF_ERROR(xmlhttprequest->forced_content_type.Set(argv[0].value.string));
	if (xmlhttprequest->GetActualReadyState() == READYSTATE_OPEN)
		CALL_FAILED_IF_ERROR(xmlhttprequest->request->ForceContentType(argv[0].value.string));

	return ES_FAILED;
}

/* virtual */ OP_STATUS
DOM_XMLHttpRequest::WaitingForHeadersThreadListener::Signal(ES_Thread *thread, ES_ThreadSignal signal)
{
	xmlhttprequest->threads_waiting_for_headers.RemoveByItem(thread);
	Remove();
	return OpStatus::OK;
}

void
DOM_XMLHttpRequest::UnblockThreadsWaitingForHeaders()
{
	unsigned count = threads_waiting_for_headers.GetCount();
	for (unsigned i = 0; i < count; i++)
		threads_waiting_for_headers.Get(i)->Unblock();

	threads_waiting_for_headers.Clear();
	headers_received = TRUE;
}

DOM_XMLHttpRequest::WaitingForHeadersThreadListener::~WaitingForHeadersThreadListener()
{
	Remove();
}

#ifdef DOM_XMLHTTPREQUEST_CONSTRAIN_SYNC_USAGE
BOOL
DOM_XMLHttpRequest::HasWindowDocument()
{
#ifdef DOM_WEBWORKERS_SUPPORT
	DOM_Object *window = GetEnvironment()->GetWindow();
	if (window->IsA(DOM_TYPE_WEBWORKERS_WORKER))
		return FALSE;
#endif // DOM_WEBWORKERS_SUPPORT

	return TRUE;
}
#endif // DOM_XMLHTTPREQUEST_CONSTRAIN_SYNC_USAGE

OP_STATUS
DOM_XMLHttpRequest::ToJSON(ES_Value *value, ES_Runtime *origining_runtime)
{
	OP_ASSERT(!responseData.IsByteBuffer());
	unsigned length = responseData.Length();
	if (length > 0)
	{
		OP_STATUS status = origining_runtime->ParseJSON(*value, responseData.GetStorage(), length);
		if (OpStatus::IsSuccess(status) || OpStatus::IsMemoryError(status))
			return status;
	}
	DOMSetNull(value);
	return OpStatus::OK;
}

#ifdef PROGRESS_EVENTS_SUPPORT
BOOL
DOM_XMLHttpRequest::HaveUploadEventListeners()
{
	if (uploader)
		return uploader->HaveEventListeners();
	else
		return FALSE;
}

OP_STATUS
DOM_XMLHttpRequest::FinishedUploading()
{
	if (!upload_complete_flag)
	{
		upload_complete_flag = TRUE;
		if (uploader)
		{
			OpFileLength bytes_total = 0;
			if (request->GetOutputStream())
				bytes_total = request->GetOutputStream()->GetContentLength();

			/* If not reported progress for the overall total, issue one 'onprogress' for it now. */
			if (uploader->GetPreviousUploadedBytes() < bytes_total)
				RETURN_IF_ERROR(uploader->SendEvent(ONPROGRESS, bytes_total, bytes_total));

			RETURN_IF_ERROR(uploader->SendEvent(ONLOAD, bytes_total, bytes_total));
			RETURN_IF_ERROR(uploader->SendEvent(ONLOADEND, bytes_total, bytes_total));
		}
	}
	return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_XMLHttpRequestUpload::Make(DOM_XMLHttpRequestUpload *&upload, DOM_Runtime *runtime)
{
	return DOMSetObjectRuntime(upload = OP_NEW(DOM_XMLHttpRequestUpload, ()), runtime, runtime->GetPrototype(DOM_Runtime::XMLHTTPREQUESTUPLOAD_PROTOTYPE), "XMLHttpRequestUpload");
}

/* virtual */ void
DOM_XMLHttpRequestUpload::GCTrace()
{
	GCMark(event_target);
}

/* virtual */ ES_GetState
DOM_XMLHttpRequestUpload::FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime *origining_runtime)
{
	ES_GetState result = DOM_Object::FetchPropertiesL(enumerator, origining_runtime);
	if (result != GET_SUCCESS)
		return result;

	enumerator->AddPropertyL("onload");
	enumerator->AddPropertyL("onerror");
	enumerator->AddPropertyL("onloadstart");
	enumerator->AddPropertyL("onloadend");
	enumerator->AddPropertyL("onprogress");
	enumerator->AddPropertyL("onabort");
	enumerator->AddPropertyL("ontimeout");

	return GET_SUCCESS;
}

/* virtual */ ES_GetState
DOM_XMLHttpRequestUpload::GetName(const uni_char *property_name, int property_code, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_name[0] == 'o' && property_name[1] == 'n')
	{
		ES_GetState res = GetEventProperty(property_name, value, static_cast<DOM_Runtime *>(origining_runtime));
		if (res != GET_FAILED)
			return res;
	}
	return DOM_Object::GetName(property_name, property_code, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_XMLHttpRequestUpload::PutName(const uni_char *property_name, int property_code, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_name[0] == 'o' && property_name[1] == 'n')
	{
		ES_PutState res = PutEventProperty(property_name, value, static_cast<DOM_Runtime *>(origining_runtime));
		if (res != PUT_FAILED)
			return res;
	}
	return DOM_Object::PutName(property_name, property_code, value, origining_runtime);
}

BOOL
DOM_XMLHttpRequestUpload::HaveEventListeners()
{
	if (DOM_EventTarget *event_target = GetEventTarget())
	{
		const DOM_EventType event_types[] = {ONLOAD, ONERROR, ONABORT, ONTIMEOUT, ONLOADSTART, ONLOADEND, ONPROGRESS};
		for (unsigned i = 0; i < ARRAY_SIZE(event_types); i++)
			if (event_target->HasListeners(event_types[i], NULL, ES_PHASE_AT_TARGET))
				return TRUE;
	}
	return FALSE;
}

OpFileLength
DOM_XMLHttpRequestUpload::GetUploadedBytes()
{
	uploaded_bytes += queued_uploaded_bytes;
	queued_uploaded_bytes = 0;

	return uploaded_bytes;
}

void
DOM_XMLHttpRequestUpload::AddUploadedBytes(unsigned bytes, BOOL queue)
{
	if (queue)
		queued_uploaded_bytes += static_cast<OpFileLength>(bytes);
	else
		uploaded_bytes += static_cast<OpFileLength>(bytes);
}

OP_STATUS
DOM_XMLHttpRequestUpload::SendEvent(DOM_EventType event_type, OpFileLength bytes_length, OpFileLength bytes_total, ES_Thread *interrupt_thread)
{
	DOM_ProgressEvent* progress_event;
	RETURN_IF_ERROR(DOMSetObjectRuntime(progress_event = OP_NEW(DOM_ProgressEvent, ()), GetRuntime(), GetRuntime()->GetPrototype(DOM_Runtime::PROGRESSEVENT_PROTOTYPE), "ProgressEvent"));
	switch (event_type)
	{
	case ONLOADEND:
	case ONLOAD:
		progress_event->InitProgressEvent(bytes_total != 0, bytes_total, bytes_total);
		break;
	default:
		OP_ASSERT(!"Unknown and unsupported; cannot happen.");
		/* 'impossible', but fallthrough. */
	case ONTIMEOUT:
	case ONABORT:
	case ONERROR:
	case ONLOADSTART:
	case ONPROGRESS:
		progress_event->InitProgressEvent(bytes_total != 0, bytes_length, bytes_total);
		break;
	}
	progress_event->InitEvent(event_type, this);
	RETURN_IF_ERROR(GetEnvironment()->SendEvent(progress_event, interrupt_thread));

	return OpStatus::OK;
}
#endif // PROGRESS_EVENTS_SUPPORT

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_XMLHttpRequest)
	DOM_FUNCTIONS_FUNCTION(DOM_XMLHttpRequest, DOM_XMLHttpRequest::open, "open", "ssbSS-")
	DOM_FUNCTIONS_FUNCTION(DOM_XMLHttpRequest, DOM_XMLHttpRequest::send, "send", 0)
	DOM_FUNCTIONS_FUNCTION(DOM_XMLHttpRequest, DOM_XMLHttpRequest::abort, "abort", 0)
	DOM_FUNCTIONS_FUNCTION(DOM_XMLHttpRequest, DOM_XMLHttpRequest::getAllResponseHeaders, "getAllResponseHeaders", 0)
	DOM_FUNCTIONS_FUNCTION(DOM_XMLHttpRequest, DOM_XMLHttpRequest::getResponseHeader, "getResponseHeader", 0)
	DOM_FUNCTIONS_FUNCTION(DOM_XMLHttpRequest, DOM_XMLHttpRequest::setRequestHeader, "setRequestHeader", "ss-")
	DOM_FUNCTIONS_FUNCTION(DOM_XMLHttpRequest, DOM_XMLHttpRequest::overrideMimeType, "overrideMimeType", "s-")
	DOM_FUNCTIONS_FUNCTION(DOM_XMLHttpRequest, DOM_Node::dispatchEvent, "dispatchEvent", 0)
DOM_FUNCTIONS_END(DOM_XMLHttpRequest)

DOM_FUNCTIONS_WITH_DATA_START(DOM_XMLHttpRequest)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_XMLHttpRequest, DOM_Node::accessEventListener, 0, "addEventListener", "s-b-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_XMLHttpRequest, DOM_Node::accessEventListener, 1, "removeEventListener", "s-b-")
DOM_FUNCTIONS_WITH_DATA_END(DOM_XMLHttpRequest)

#ifdef PROGRESS_EVENTS_SUPPORT
DOM_FUNCTIONS_START(DOM_XMLHttpRequestUpload)
	DOM_FUNCTIONS_FUNCTION(DOM_XMLHttpRequestUpload, DOM_Node::dispatchEvent, "dispatchEvent", 0)
DOM_FUNCTIONS_END(DOM_XMLHttpRequestUpload)

DOM_FUNCTIONS_WITH_DATA_START(DOM_XMLHttpRequestUpload)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_XMLHttpRequestUpload, DOM_Node::accessEventListener, 0, "addEventListener", "s-b-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_XMLHttpRequestUpload, DOM_Node::accessEventListener, 1, "removeEventListener", "s-b-")
DOM_FUNCTIONS_WITH_DATA_END(DOM_XMLHttpRequestUpload)
#endif // PROGRESS_EVENTS_SUPPORT

DOM_XMLHttpRequest_Constructor::DOM_XMLHttpRequest_Constructor()
	: DOM_BuiltInConstructor(DOM_Runtime::XMLHTTPREQUEST_PROTOTYPE)
{
}

/* virtual */ int
DOM_XMLHttpRequest_Constructor::Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime)
{
	DOM_XMLHttpRequest *xmlhttprequest;
	CALL_FAILED_IF_ERROR(DOM_XMLHttpRequest::Make(xmlhttprequest, GetEnvironment()));

	DOMSetObject(return_value, xmlhttprequest);
	return ES_VALUE;
}

DOM_AnonXMLHttpRequest_Constructor::DOM_AnonXMLHttpRequest_Constructor()
	: DOM_BuiltInConstructor(DOM_Runtime::ANONXMLHTTPREQUEST_PROTOTYPE)
{
}

/* virtual */ int
DOM_AnonXMLHttpRequest_Constructor::Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime)
{
	DOM_XMLHttpRequest *anonxmlhttprequest;
	CALL_FAILED_IF_ERROR(DOM_XMLHttpRequest::Make(anonxmlhttprequest, GetEnvironment(), TRUE));

	DOMSetObject(return_value, anonxmlhttprequest);
	return ES_VALUE;
}

#endif // DOM_HTTP_SUPPORT
