/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef HAS_SET_HTTP_DATA
#include "modules/upload/upload.h"
#include "modules/url/url_man.h"
#include "modules/viewers/viewers.h"

#include "modules/olddebug/tstdump.h"
#include "modules/url/tools/arrays.h"
#include "modules/encodings/utility/opstring-encodings.h"

#include "modules/content_filter/content_filter.h"

Upload_URL::Upload_URL()
{
	desc = NULL;
	force_filename = TRUE;
	force_qp_content_type_name = FALSE;
	m_finished = FALSE;
	upload_start_offset = 0;
	upload_length = 0;
}

Upload_URL::~Upload_URL()
{
	OP_DELETE(desc);
	desc = NULL;
}

void Upload_URL::InitL(URL &url, const OpStringC &suggested_name, const OpStringC8 &encoded_charset, MIME_Encoding enc, const char **header_names)
{
	InitL(url, suggested_name, NULL, NULL, encoded_charset, enc, header_names);
}

void Upload_URL::InitL(const OpStringC &url_name, const OpStringC &suggested_name, const OpStringC8 &encoded_charset, MIME_Encoding enc, const char **header_names)
{
	InitL(url_name, suggested_name, NULL, NULL, encoded_charset, enc, header_names);
}

PREFIX_CONST_ARRAY(static, const char*, headers_def, upload)
  CONST_ENTRY("Content-Disposition")
  CONST_ENTRY("Content-Type")
  CONST_ENTRY(NULL)
CONST_END(headers_def)

void Upload_URL::InitL(URL &url, const OpStringC &suggested_name, const OpStringC8 &disposition, const OpStringC8 &disposition_name, const OpStringC8 &encoded_charset, MIME_Encoding enc, const char **header_names)
{
#if defined(URL_FILTER) && defined(UPLOAD_URL_FILTER_RULES)
	BOOL ret = FALSE;
	OpStatus::Ignore(g_urlfilter->CheckURL(&url, ret, NULL));
	if (!ret)
		return;
#endif // URL_FILTER && UPLOAD_URL_FILTER_RULES

	src_url = url;
	if(!src_url->IsEmpty())
	{
		src_url->QuickLoad(FALSE);

		OP_ASSERT(src_url->GetAttribute(URL::KLoadStatus, URL::KFollowRedirect) == URL_LOADED);
	}

	Upload_Handler::InitL(enc, header_names ?  header_names : (const char **)g_headers_def);

	suggested_filename.SetL(suggested_name);

	OpString8 mime_type;
	ANCHOR(OpString8, mime_type);

	src_url->GetAttributeL(URL::KMIME_Type, mime_type);

	if(mime_type.IsEmpty())
	{
		OpString ext;
		ANCHOR(OpString, ext);
		const uni_char *fileext = NULL;

		src_url->GetAttributeL(URL::KSuggestedFileNameExtension_L, ext);
		if(ext.HasContent())
			fileext = ext.CStr();
		else
			fileext = FindFileExtension(suggested_filename.CStr());

		if (fileext && *fileext)
		{
			Viewer* viewer = g_viewers->FindViewerByExtension(fileext);
			if (viewer)
				OpStatus::Ignore(mime_type.Set(viewer->GetContentTypeString8())); // not really critical
		}
	}

	SetContentTypeL(mime_type.HasContent() ? mime_type.CStr() : "application/octet-stream");

	SetContentDispositionL(disposition.HasContent() ? disposition.CStr() : "attachment");
	if(disposition_name.HasContent())
		SetContentDispositionParameterL("name", disposition_name, TRUE);

	SetCharsetL(src_url->GetAttribute(URL::KMIME_CharSet, URL::KFollowRedirect));
	encoding_charset.SetL(encoded_charset);

	OpFileLength registered_len = 0;
	src_url->GetAttribute(URL::KContentSize, &registered_len);
	if(upload_length > 0)
		registered_len = MIN(upload_length, registered_len);

	if(registered_len > UPLOAD_LONG_BINARY_BUFFER_SIZE)
		SetMinimumBufferSize(UPLOAD_MIN_BUFFER_SIZE_WHEN_LONG);
}

void Upload_URL::InitL(const OpStringC &url_name, const OpStringC &suggested_name, const OpStringC8 &disposition, const OpStringC8 &disposition_name, const OpStringC8 &encoded_charset, MIME_Encoding enc, const char **header_names)
{
	OpString resolved_name;
	ANCHOR(OpString, resolved_name);
	URL src;
	ANCHOR(URL, src);

	ResolveUrlNameL(url_name, resolved_name);
#if defined _LOCALHOST_SUPPORT_ || defined _FILE_UPLOAD_SUPPORT_
	// The Upload_URL::Init method doesn't take a filename but a "url
	// name" which is some kind of unescaped filename. So to be able
	// to handle filenames like "My%20File.txt" and filenames like
	// "\My Path\My%20File.txt" we have to escape it after the
	// filename was resolved (escaping would replace "\" by "%5C", but
	// resolving the filename first will replace "\" by "/" which
	// remains the same in the escaped version.
	size_t value_len = resolved_name.Length();
	OpString escaped_filename_obj;
	ANCHOR(OpString, escaped_filename_obj);
	uni_char* escaped_filename = escaped_filename_obj.ReserveL((int)(value_len*3+1));	//worst case
	EscapeFileURL(escaped_filename, const_cast<uni_char*>(resolved_name.CStr())); // it won't touch |value|
	src = urlManager->GetURL(escaped_filename_obj.CStr(), NULL, TRUE);
#else
	src = urlManager->GetURL(resolved_name.CStr(), NULL, TRUE);
#endif

	InitL(src, suggested_name, disposition, disposition_name, encoded_charset, enc, header_names);
}

void Upload_URL::SetUploadRange(OpFileLength offset, OpFileLength length)
{
	OP_ASSERT(desc == NULL);

	upload_start_offset = offset;
	upload_length = length;
}

uint32 Upload_URL::PrepareL(Boundary_List &boundaries, Upload_Transfer_Mode transfer_restrictions)
{
	if (force_filename)
	{
		OpString suggested_name1;
		ANCHOR(OpString, suggested_name1);
		const uni_char *name_candidate = suggested_filename.CStr();
			
		if(!name_candidate)
		{
			src_url->GetAttributeL(URL::KSuggestedFileName_L, suggested_name1, URL::KFollowRedirect);
			name_candidate = suggested_name1.CStr();
		}

		if(name_candidate == NULL || *name_candidate == '\0')
		{
#if defined _LOCALHOST_SUPPORT_ || !defined RAMCACHE_ONLY
			name_candidate = src_url->GetAttribute(URL::KFileName, URL::KFollowRedirect).CStr();
			if(name_candidate == NULL)
#endif
				name_candidate = UNI_L("");
		}

		OpStringC8 actual_charset = NULL;
		OpString8 filename;
		ANCHOR(OpString8, filename);
		if (SetToEncodingL(&filename, "us-ascii", name_candidate) != 0)
		{
			actual_charset = encoding_charset;
			if (actual_charset.IsEmpty() || actual_charset.CompareI("utf-16") == 0 ||
				SetToEncodingL(&filename, actual_charset.CStr(), name_candidate) != 0)
			{
				actual_charset = "utf-8";
				SetToEncodingL(&filename, actual_charset.CStr(), name_candidate);
			}
		}
		
#if defined URL_UPLOAD_RFC_2231_SUPPORT || defined URL_UPLOAD_QP_SUPPORT
		if(transfer_restrictions == UPLOAD_BINARY_NO_CONVERSION)
#endif
		{
			SetContentDispositionParameterL("filename", (filename.HasContent() ? filename.CStr() : ""), TRUE);
		}
#if defined URL_UPLOAD_RFC_2231_SUPPORT || defined URL_UPLOAD_QP_SUPPORT
		else
		{
			if (GetEncoding()==ENCODING_AUTODETECT &&
				(transfer_restrictions == UPLOAD_7BIT_TRANSFER || transfer_restrictions == UPLOAD_8BIT_TRANSFER) && 
				content_type.CompareI("application/",STRINGLENGTH("application/"))==0)
				SetEncoding(ENCODING_BASE64);

#ifdef URL_UPLOAD_RFC_2231_SUPPORT
			AccessHeaders().AddRFC2231ParameterL("Content-Disposition", "filename", filename, actual_charset);
#ifdef URL_UPLOAD_QP_SUPPORT
			if (force_qp_content_type_name)
				AccessHeaders().AddQuotedPrintableParameterL("Content-Type", "name", filename, actual_charset, ENCODING_AUTODETECT);
			else
#endif
				AccessHeaders().AddRFC2231ParameterL("Content-Type", "name", filename, actual_charset);
#elif defined URL_UPLOAD_QP_SUPPORT
			AccessHeaders().AddQuotedPrintableParameterL("Content-Disposition", "filename", filename, actual_charset, ENCODING_AUTODETECT);
			AccessHeaders().AddQuotedPrintableParameterL("Content-Type", "name", filename, actual_charset, ENCODING_AUTODETECT);
#endif
		}
#endif
	}
	return Upload_Handler::PrepareL(boundaries, transfer_restrictions);
}

void Upload_URL::ResetL()
{
	Upload_Handler::ResetL();
	OP_DELETE(desc);
	desc = NULL;
	m_finished = FALSE;
}

void Upload_URL::Release()
{
	OP_DELETE(desc);
	desc = NULL;
	src_url.UnsetURL();
}

uint32 Upload_URL::GetNextContentBlock(unsigned char *a_buf, uint32 a_max_len, BOOL &more)
{
	more = FALSE;

	BOOL first_block = desc == NULL;

	if(!CheckDescriptor())
		return 0;

	unsigned char * OP_MEMORY_VAR buf = a_buf;
	OP_MEMORY_VAR uint32 max_len = a_max_len;
	OP_MEMORY_VAR uint32 len =0;
	OP_MEMORY_VAR uint32 buf_len=0;

	if (first_block && upload_start_offset > 0)
		desc->SetPosition(upload_start_offset);

	while(max_len > 0)
	{
		TRAPD(err, buf_len = desc->RetrieveDataL(more));//FIXME:OOM Error handling needed

		if(buf_len == 0 || desc->GetBuffer() == NULL)
			break;

		if(buf_len > max_len)
			buf_len = max_len;

		op_memcpy(buf, desc->GetBuffer(), buf_len);

		desc->ConsumeData(buf_len);

		buf += buf_len;
		max_len -= buf_len;
		len += buf_len;
	}

	if(!more && desc->GetBufSize() != 0)
		more = TRUE;


	if(!more)
	{
		OP_DELETE(desc);
		desc = NULL;
		m_finished = TRUE;
	}

	return len;	
}

void Upload_URL::ResetContent()
{
	OP_DELETE(desc);
	desc = NULL;
	m_finished = FALSE;
}

OpFileLength Upload_URL::PayloadLength()
{
	OpFileLength registered_len=0;

	if(!src_url->IsEmpty())
		src_url->GetAttribute(URL::KContentLoaded_Update, &registered_len);

	return upload_length > 0 ? MIN(upload_length, registered_len) : registered_len;
}

BOOL Upload_URL::DecodeToCalculateLength()
{
#ifdef _HTTP_COMPRESS
	OpString enc;
	if (!src_url->IsEmpty())
		src_url->GetAttribute(URL::KHTTPEncoding, enc);
	if (enc.Find("gzip")==0 || enc.Find("deflate")==0 || enc.Find("compress")==0)
		return TRUE;
#endif
	return FALSE;
}

BOOL Upload_URL::CheckDescriptor()
{
	if(desc)
		return TRUE;

	if(src_url->IsEmpty())
		return FALSE;

	desc = src_url->GetDescriptor(NULL, URL::KFollowRedirect, TRUE, TRUE);

	return (desc ? TRUE : FALSE);
}

uint32 Upload_URL::FileCount()
{
	return 1;
}
      
uint32 Upload_URL::UploadedFileCount()
{
	return (m_finished ? 1 : 0);
}

URLContentType Upload_URL::GetSrcContentType()
{
	return src_url->ContentType();
}

void Upload_TemporaryURL::InitL(URL &url, const OpStringC &suggested_name, BOOL contain_header, const OpStringC8 &encoded_charset, MIME_Encoding enc, const char **header_names)
{
	URL target = urlManager->GetNewOperaURL();
	ANCHOR(URL, target);
	URL src;
	ANCHOR(URL, src);
	src = url.GetAttribute(URL::KMovedToURL, URL::KFollowRedirect);
	
	g_url_api->MakeUnique(target);

	if(src.IsEmpty())
		src = url;

	if(!src.IsEmpty())
		src.QuickLoad(FALSE);

	target.SetAttributeL(URL::KMIME_ForceContentType, src.GetAttribute(URL::KMIME_Type));
	target.SetAttributeL(URL::KMIME_CharSet, src.GetAttribute(URL::KMIME_CharSet));

	OpStackAutoPtr<URL_DataDescriptor> desc (src.GetDescriptor(NULL, URL::KFollowRedirect, TRUE));
	LEAVE_IF_NULL(desc.get());

	if(desc.get())
	{
		if(contain_header)
		{
			BOOL more = FALSE;
			unsigned long len = desc->RetrieveData(more);
			uint32 h_len = ExtractHeadersL((unsigned char *) desc->GetBuffer(), len);
				
			desc->ConsumeData(h_len);
		}

		target.WriteDocumentData(URL::KNormal, desc.get());
		target.WriteDocumentDataFinished();
	}

	desc.reset();

	Upload_URL::InitL(target, suggested_name, encoded_charset, enc, header_names);
}

void Upload_TemporaryURL::InitL(const OpStringC &url_name, const OpStringC &suggested_name, BOOL contain_header, const OpStringC8 &encoded_charset, MIME_Encoding enc, const char **header_names)
{
	OpString resolved_name;
	ANCHOR(OpString, resolved_name);

	ResolveUrlNameL(url_name, resolved_name);
	URL src = urlManager->GetURL(resolved_name.CStr());

	InitL(src, suggested_name, contain_header, encoded_charset, enc, header_names);
}

#endif // HTTP_data
