/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"
#include "modules/url/tools/content_detector.h"
#include "modules/url/url_ds.h"
#include "modules/url/tools/arrays.h"
#include "modules/viewers/viewers.h"
#include "modules/encodings/detector/charsetdetector.h"

/* Implements http://mimesniff.spec.whatwg.org/ */

/* sniff table

+-------------------+-------------------+-----------------+------------+
| Mask in Hex       | Pattern in Hex    | Sniffed Type    | Security   |
+-------------------+-------------------+-----------------+------------+
| FF FF FF DF DF DF | WS 3C 21 44 4F 43 | text/html       | Scriptable |
| DF DF DF DF FF DF | 54 59 50 45 20 48 |                 |            |
| DF DF DF FF       | 54 4D 4C _>       |                 |            |
| Comment: <!DOCTYPE HTML                                              |
+-------------------+-------------------+-----------------+------------+
| FF FF DF DF DF DF | WS 3C 48 54 4D 4C | text/html       | Scriptable |
| FF                | _>                |                 |            |
| Comment: <HTML                                                       |
+-------------------+-------------------+-----------------+------------+
| FF FF DF DF DF DF | WS 3C 48 45 41 44 | text/html       | Scriptable |
| FF                | _>                |                 |            |
| Comment: <HEAD                                                       |
+-------------------+-------------------+-----------------+------------+
| FF FF DF DF DF DF | WS 3C 53 43 52 49 | text/html       | Scriptable |
| DF DF FF          | 50 54 _>          |                 |            |
| Comment: <SCRIPT                                                     |
+-------------------+-------------------+-----------------+------------+
| FF FF DF DF DF DF | WS 3C 49 46 52 41 | text/html       | Scriptable |
| DF DF FF          | 4d 45 _>          |                 |            |
| Comment: <IFRAME                                                     |
+-------------------+-------------------+-----------------+------------+
| FF FF DF FF FF    | WS 3C 48 31 _>    | text/html       | Scriptable |
| Comment: <H1                                                         |
+-------------------+-------------------+-----------------+------------+
| FF FF DF DF DF FF | WS 3C 44 49 56 _> | text/html       | Scriptable |
| Comment: <DIV                                                        |
+-------------------+-------------------+-----------------+------------+
| FF FF DF DF DF DF | WS 3C 46 4f 4e 54 | text/html       | Scriptable |
| FF                | _>                |                 |            |
| Comment: <FONT                                                       |
+-------------------+-------------------+-----------------+------------+
| FF FF DF DF DF DF | WS 3C 54 41 42 4c | text/html       | Scriptable |
| DF FF             | 45 _>             |                 |            |
| Comment: <TABLE                                                      |
+-------------------+-------------------+-----------------+------------+
| FF FF DF FF       | WS 3C 41 _>       | text/html       | Scriptable |
| Comment: <A                                                          |
+-------------------+-------------------+-----------------+------------+
| FF FF DF DF DF DF | WS 3C 53 54 59 4c | text/html       | Scriptable |
| DF FF             | 45 _>             |                 |            |
| Comment: <STYLE                                                      |
+-------------------+-------------------+-----------------+------------+
| FF FF DF DF DF DF | WS 3C 54 49 54 4c | text/html       | Scriptable |
| DF FF             | 45 _>             |                 |            |
| Comment: <TITLE                                                      |
+-------------------+-------------------+-----------------+------------+
| FF FF DF FF       | WS 3C 42 _>       | text/html       | Scriptable |
| Comment: <B                                                          |
+-------------------+-------------------+-----------------+------------+
| FF FF DF DF DF DF | WS 3C 42 4f 44 59 | text/html       | Scriptable |
| FF                | _>                |                 |            |
| Comment: <BODY                                                       |
+-------------------+-------------------+-----------------+------------+
| FF FF DF DF FF    | WS 3C 42 52 _>    | text/html       | Scriptable |
| Comment: <BR                                                         |
+-------------------+-------------------+-----------------+------------+
| FF FF DF FF       | WS 3C 50 _>       | text/html       | Scriptable |
| Comment: <P                                                          |
+-------------------+-------------------+-----------------+------------+
| FF FF FF FF FF FF | WS 3C 21 2d 2d _> | text/html       | Scriptable |
| Comment: <!--                                                        |
+-------------------+-------------------+-----------------+------------+
| FF FF FF FF FF FF | WS 3C 3f 78 6d 6c | text/xml        | Scriptable |
| Comment: <?xml (Note the case sensitivity and lack of trailing _>)   |
+-------------------+-------------------+-----------------+------------+
| FF FF FF FF FF    | 25 50 44 46 2D    | application/pdf | Scriptable |
| Comment: The string "%PDF-", the PDF signature.                      |
+-------------------+-------------------+-----------------+------------+
| FF FF FF FF FF FF | 25 21 50 53 2D 41 | application/    | Safe       |
| FF FF FF FF FF    | 64 6F 62 65 2D    |      postscript |            |
| Comment: The string "%!PS-Adobe-", the PostScript signature.         |
+-------------------+-------------------+-----------------+------------+
| FF FF 00 00       | FE FF 00 00       | text/plain      | n/a        |
| Comment: UTF-16BE BOM                                                |
+-------------------+-------------------+-----------------+------------+
| FF FF 00 00       | FF FE 00 00       | text/plain      | n/a        |
| Comment: UTF-16LE BOM                                                |
+-------------------+-------------------+-----------------+------------+
| FF FF FF 00       | EF BB BF 00       | text/plain      | n/a        |
| Comment: UTF-8 BOM                                                   |
+-------------------+-------------------+-----------------+------------+
| FF FF FF FF FF FF | 47 49 46 38 37 61 | image/gif       | Safe       |
| Comment: The string "GIF87a", a GIF signature.                       |
+-------------------+-------------------+-----------------+------------+
| FF FF FF FF FF FF | 47 49 46 38 39 61 | image/gif       | Safe       |
| Comment: The string "GIF89a", a GIF signature.                       |
+-------------------+-------------------+-----------------+------------+
| FF FF FF FF FF FF | 89 50 4E 47 0D 0A | image/png       | Safe       |
| FF FF             | 1A 0A             |                 |            |
| Comment: The PNG signature.                                          |
+-------------------+-------------------+-----------------+------------+
| FF FF FF          | FF D8 FF          | image/jpeg      | Safe       |
| Comment: A JPEG SOI marker followed by a octet of another marker.    |
+-------------------+-------------------+-----------------+------------+
| FF FF             | 42 4D             | image/bmp       | Safe       |
| Comment: The string "BM", a BMP signature.                           |
+-------------------+-------------------+-----------------+------------+
| FF FF FF FF 00 00 | 52 49 46 46 00 00 | image/webp      | Safe       |
| 00 00 FF FF FF FF | 00 00 57 45 42 50 |                 |            |
| FF FF             | 56 50             |                 |            |
| Comment: "RIFF" followed by four bytes, followed by "WEBPVP".        |
+-------------------+-------------------+-----------------+------------+
| FF FF FF FF       | 00 00 01 00       | image/vnd.      | Safe       |
|                   |                   |  microsoft.icon |            |
| Comment: A Windows Icon signature.                                   |
+-------------------+-------------------+-----------------+------------+
| FF FF FF FF FF    | 4F 67 67 53 00    | application/ogg | Safe       |
| Comment: An Ogg audio or video signature.                            |
+-------------------+-------------------+-----------------+------------+
| FF FF FF FF 00 00 | 52 49 46 46 00 00 | audio/wave      | Safe       |
| 00 00 FF FF FF FF | 00 00 57 41 56 45 |                 |            |
| Comment: "RIFF" followed by four bytes, followed by "WAVE".          |
+-------------------+-------------------+-----------------+------------+
| FF FF FF FF       | 1A 45 DF A3       | video/webm      | Safe       |
| Comment: The WebM signature [TODO: Use more octets?]                 |
+-------------------+-------------------+-----------------+------------+
| FF FF FF FF FF FF | 52 61 72 20 1A 07 | application/    | Safe       |
| FF                | 00                | x-rar-compressed|            |
| Comment: A RAR archive.                                              |
+-------------------+-------------------+-----------------+------------+
| FF FF FF FF       | 50 4B 03 04       | application/zip | Safe       |
| Comment: A ZIP archive.                                              |
+-------------------+-------------------+-----------------+------------+
| FF FF FF          | 1F 8B 08          | application/    | Safe       |
|                   |                   |          x-gzip |            |
| Comment: A GZIP archive.                                             |
+-------------------+-------------------+-----------------+------------+
 */

#define WS 0x20 // whitespace octet (WS is spec)
#define SB 0x3E	// space-or-bracket octet ("_>" is spec)

#ifdef NEED_URL_EXTERNAL_GET_MIME_FROM_SAMPLE
extern void GetMimeTypeFromDataSampleL(const OpStringC&aFileName,const char* aBuffer,int aLength,OpString8& aMimeType);
#endif

ContentDetector::ContentDetector(URL_Rep *url, BOOL dont_leave_undetermined, BOOL extension_use_allowed, CompliancyType compliancy_type /*= WEB*/):
	m_octets(0),
	m_length(0),
	m_deterministic(TRUE),
	m_dont_leave_undetermined(dont_leave_undetermined),
	m_extension_use_allowed(extension_use_allowed),
	m_official_content_type(URL_UNDETERMINED_CONTENT),
	m_compliancy_type(compliancy_type),
	m_url(url)
{
}

ContentDetector::~ContentDetector()
{
}

OP_STATUS ContentDetector::DetectContentType()
{
	m_official_content_type = (URLContentType)m_url->GetAttribute(URL::KContentType, TRUE);

	UINT32 port = m_url->GetAttribute(URL::KResolvedPort);
	URLType url_type =  static_cast<URLType>(m_url->GetAttribute(URL::KType));

	if (((url_type == URL_HTTP && port != 80) || (url_type == URL_HTTPS && port != 443)) && !m_url->GetAttribute(URL::KHTTP_10_or_more))
	{
		// Do not sniff content-type for HTTP 0.9 servers on ports other than 80 for HTTP or 443 for HTTPS (CORE-35973).
		if (m_official_content_type == URL_UNDETERMINED_CONTENT)
			RETURN_IF_ERROR(m_url->SetAttribute(URL::KContentType, URL_TEXT_CONTENT));

		return OpStatus::OK;
	}

	RETURN_IF_ERROR(m_url->GetAttribute(URL::KOriginalMIME_Type, m_official_mime_type));
	int mime_length = m_official_mime_type.Length();

	URLContentType sniffed_content_type = m_official_content_type;
	OpStringC8 sniffed_mime_type;

	OpAutoPtr<URL_DataDescriptor> desc(m_url->GetDescriptor(NULL, URL::KFollowRedirect, TRUE));
	if (!desc.get())
	{
		m_octets = NULL;
		m_length = 0;
	}
	else
	{
		BOOL more;
		RETURN_IF_LEAVE(m_length = desc->RetrieveDataL(more));
		m_octets = desc->GetBuffer();
	}

#ifdef NEED_URL_EXTERNAL_GET_MIME_FROM_SAMPLE
	if (CheckExternally())
		return OpStatus::OK;
#endif

	if (m_url->GetAttribute(URL::KMultimedia))
	{
		RETURN_IF_ERROR(IsVideo(sniffed_content_type, sniffed_mime_type));
	}
	else if (m_official_mime_type.CompareI("text/plain") == 0)
	{
		RETURN_IF_ERROR(IsTextOrBinary(sniffed_content_type, sniffed_mime_type));
	}
	else if (m_official_mime_type.IsEmpty() ||
			 m_official_mime_type.CompareI("unknown/unknown") == 0 ||
			 m_official_mime_type.CompareI("application/unknown") == 0 ||
			 m_official_mime_type.CompareI("*/*") == 0)
	{
		RETURN_IF_ERROR(IsUnknownType(sniffed_content_type, sniffed_mime_type));
	}
	else if (m_official_mime_type.FindI("+xml") == mime_length - 4 ||
			 m_official_mime_type.CompareI("text/xml") == 0	||
			 m_official_mime_type.CompareI("application/xml") == 0)
	{
		return OpStatus::OK;
	}
	else if (m_official_mime_type.CompareI("image/", 6) == 0)
	{
		RETURN_IF_ERROR(IsImage(sniffed_content_type, sniffed_mime_type));
	}

	if (m_dont_leave_undetermined && sniffed_content_type == URL_UNDETERMINED_CONTENT)
	{
		sniffed_content_type = URL_UNKNOWN_CONTENT;
		m_url->SetAttribute(URL::KUntrustedContent, TRUE);
		sniffed_mime_type = "application/octet-stream";
	}

	if (sniffed_content_type != m_official_content_type)
	{
		/* CORE-39801: If we sniffed a container format (such as ZIP or GZIP),
		 * check if the URL string suggests a file extension of a known format
		 * using that format as its container. If so, allow it through if we
		 * originally did not get a valid Content-Type. */
		OpString fileext;
		TRAPD(err, m_url->GetAttributeL(URL::KUniNameFileExt_L, fileext, URL::KFollowRedirect));
		if (OpStatus::IsSuccess(err) && !fileext.IsEmpty())
		{
			Viewer *viewer = NULL;
			OP_STATUS rc = g_viewers->FindViewerByExtension(fileext, viewer);
			if (OpStatus::IsSuccess(rc) && viewer && viewer->AllowedContainer()
				&& 0 == op_strcmp(viewer->AllowedContainer(), sniffed_mime_type.CStr()))
			{
				sniffed_content_type = viewer->GetContentType();
				sniffed_mime_type = viewer->GetContentTypeString8();
			}
			else if (OpStatus::IsMemoryError(rc))
			{
				/* Propagate out-of-memory errors only; any other error just
				 * means we didn't find a Viewer object. */
				return rc;
			}
		}

		RETURN_IF_ERROR(m_url->SetAttribute(URL::KContentType, sniffed_content_type));
		RETURN_IF_ERROR(m_url->SetAttribute(URL::KUntrustedContent, TRUE));
		RETURN_IF_ERROR(m_url->SetAttribute(URL::KMIME_Type, sniffed_mime_type));
	}

	return OpStatus::OK;
}

#ifdef NEED_URL_EXTERNAL_GET_MIME_FROM_SAMPLE

BOOL ContentDetector::CheckExternally()
{
#ifdef NEED_EPOC_CONTENT_TYPE
	if (m_official_content_type == URL_EPOC_CONTENT)
		return TRUE;
#endif
	OpString8 mime_type;
	TRAPD(err,GetMimeTypeFromDataSampleL(m_url->UniName(PASSWORD_HIDDEN), m_octets, m_length, mime_type));
	if (OpStatus::IsSuccess(err) && mime_type.Length())
	{
		if (OpStatus::IsSuccess(m_url->SetAttribute(URL::KMIME_ForceContentType, mime_type)))
		{
			URLContentType rc = m_official_content_type;
#ifdef NEED_EPOC_CONTENT_TYPE
			if (URL_UNKNOWN_CONTENT == rc)
				RETURN_IF_ERROR(m_url->SetAttribute(URL::KContentType, rc = URL_EPOC_CONTENT)); // To avoid being called again
#endif
			if( URL_TEXT_CONTENT != rc )
				return TRUE;
		}
	}
	return FALSE;
}

#endif // NEED_URL_EXTERNAL_GET_MIME_FROM_SAMPLE

OP_STATUS ContentDetector::IsUnknownType(URLContentType &url_type, OpStringC8 &mime_type)
{
	unsigned long length = MIN(m_length, DETERMINISTIC_HEADER_LENGTH);

	int found_at_index;
	RETURN_IF_ERROR(LookUpInSniffTable(m_octets, length, FALSE, TRUE, FALSE, UNDETERMINED, found_at_index));
	if (found_at_index >= 0)
	{
		ContentDetectorPatternData pd = GetPatternData(found_at_index);

		// if the found content type is not displayable then try to find by extension (if feature enabled)
		if ((m_extension_use_allowed || m_compliancy_type == WEB) && !IsDisplayable(pd.content_type))
		{
			BOOL found = FALSE;
			RETURN_IF_ERROR(TryToFindByExtension(url_type, mime_type, found));
			if (found)
				return OpStatus::OK;
		}

		url_type = pd.content_type;
		mime_type = GetMimeType(found_at_index);
		return OpStatus::OK;
	}

	if (m_compliancy_type == WEB)
	{
		int image_type = imgManager->CheckImageType((const unsigned char *)m_octets, length);
		if (image_type > 0)
		{
			url_type = (URLContentType)image_type;
			const char *mimetype = g_viewers->GetContentTypeString(url_type);
			mime_type = mimetype ? mimetype : "image/unknown";
			return OpStatus::OK;
		}
	}

	if (HasMP4Signature(m_octets, length))
	{
		url_type = URL_MEDIA_CONTENT;
		mime_type = "video/mp4";
		return OpStatus::OK;
	}

	if (HasBinaryOctet(m_octets, length))
	{
		BOOL found = FALSE;
		if (m_extension_use_allowed || m_compliancy_type == WEB)
			TryToFindByExtension(url_type, mime_type, found);

		if (!found)
		{
			url_type = URL_UNKNOWN_CONTENT;
			mime_type = "application/octet-stream";
		}
	}
	else
	{
		m_deterministic = m_length >= DETERMINISTIC_HEADER_LENGTH;
		url_type = URL_TEXT_CONTENT;
		mime_type = "text/plain";
	}
	return OpStatus::OK;
}

OP_STATUS ContentDetector::TryToFindByExtension(URLContentType &url_type, OpStringC8 &mime_type, BOOL &found)
{
	OpString path;
	URLContentType ret_url_type = URL_UNDETERMINED_CONTENT;
	found = FALSE;
	if (OpStatus::IsSuccess(m_url->GetAttribute(URL::KUniPath, 0, path)))
		RETURN_IF_ERROR(m_url->GetDataStorage()->FindContentType(ret_url_type, NULL, 0, path.CStr(), TRUE));
	if (ret_url_type != URL_UNDETERMINED_CONTENT)
	{
		url_type = ret_url_type;
		const char *mimetype = g_viewers->GetContentTypeString(url_type);
		mime_type = mimetype ? mimetype : "application/octet-stream";
		found = TRUE;
	}
	return OpStatus::OK;
}

BOOL ContentDetector::IsDisplayable(URLContentType url_type)
{
	for (int index = 0; index < NUMBER_OF_MIMETYPES; index++)
	{
		ContentDetectorPatternData pd = GetPatternData(index);
		if (pd.content_type == url_type &&
			(pd.simple_type == IMAGE || pd.simple_type == VIDEO ||
			 pd.simple_type == TEXT || pd.simple_type == HTML))
			return TRUE;
	}
	return FALSE;
}

OP_STATUS ContentDetector::IsTextOrBinary(URLContentType &url_type, OpStringC8 &mime_type)
{
	unsigned long length = MIN(m_length, DETERMINISTIC_HEADER_LENGTH);

	const unsigned char UTF16BE_BOM[2] = {0xFE, 0xFF};
	const unsigned char UTF16LE_BOM[2] = {0xFF, 0xFE};
	const unsigned char UTF8_BOM[3]    = {0xEF, 0xBB, 0xBF};

	if (length >= 3)
	{
		if (op_memcmp(m_octets, UTF16BE_BOM, 2) == 0 ||
			op_memcmp(m_octets, UTF16LE_BOM, 2) == 0 ||
			op_memcmp(m_octets, UTF8_BOM, 3) == 0)
		{
			url_type = URL_TEXT_CONTENT;
			mime_type = "text/plain";
			return OpStatus::OK;
		}
	}

	if (!HasBinaryOctet(m_octets, length))
	{
		m_deterministic = m_length >= DETERMINISTIC_HEADER_LENGTH;
		url_type = URL_TEXT_CONTENT;
		mime_type = "text/plain";
		return OpStatus::OK;
	}

	if (m_compliancy_type == WEB)
	{
		CharsetDetector detector(NULL);
		detector.PeekAtBuffer(m_octets, length);
		if (detector.GetDetectedCharset())
		{
			url_type = URL_TEXT_CONTENT;
			mime_type = "text/plain";
		}
	}

	int found_at_index;
	RETURN_IF_ERROR(LookUpInSniffTable(m_octets, length, TRUE, TRUE, FALSE, UNDETERMINED, found_at_index));
	if (found_at_index >= 0)
	{
		ContentDetectorPatternData pd = GetPatternData(found_at_index);
		url_type = pd.content_type;
		mime_type = GetMimeType(found_at_index);
	}
	else
	{
		url_type = URL_UNKNOWN_CONTENT;
		mime_type = "application/octet-stream";
	}
	return OpStatus::OK;
}

BOOL ContentDetector::HasBinaryOctet(const char *octets, unsigned long octets_length)
{
	for (unsigned long index = 0; index < octets_length; index ++)
	{
		unsigned char octet = octets[index];
		if (octet <= 0x08 || octet == 0x0B  || (octet >= 0x0E && octet <= 0x1A)  || (octet >= 0x1C && octet <= 0x1F))
			return TRUE;
	}
	return FALSE;
}

BOOL ContentDetector::HasMP4Signature(const char *octets, unsigned long octets_length)
{
	const char ftyp[4] = {0x66, 0x74, 0x79, 0x70}; // the ASCII string "ftyp"
	const char mp4[3] = {0x6D, 0x70, 0x34}; // the ASCII string "mp4"

	if (octets_length >= 12)
	{
		const UINT32 box_size = (octets[0] << 24) + (octets[1] << 16) + (octets[2] << 8) + octets[3];
		if (octets_length >= box_size && box_size % 4 == 0 &&
			op_memcmp(&octets[4], ftyp, 4) == 0)
		{
			for (UINT32 i = 2; i <= box_size / 4 - 1; i++)
			{
				if (i != 3 && op_memcmp(&octets[4*i], mp4, 3) == 0)
					return TRUE;
			}
		}
	}
	return FALSE;
}

OP_STATUS ContentDetector::IsImage(URLContentType &url_type, OpStringC8 &mime_type)
{
	if (m_official_mime_type.CompareI("image/svg+xml") == 0)
		return OpStatus::OK;
	int found_at_index;
	RETURN_IF_ERROR(LookUpInSniffTable(m_octets, m_length, FALSE, TRUE, TRUE, IMAGE, found_at_index));
	if (found_at_index >= 0)
	{
		ContentDetectorPatternData pd = GetPatternData(found_at_index);
		url_type = pd.content_type;
		mime_type = GetMimeType(found_at_index);
	}
	return OpStatus::OK;
}

OP_STATUS ContentDetector::IsVideo(URLContentType &url_type, OpStringC8 &mime_type)
{
	unsigned long length = MIN(m_length, DETERMINISTIC_HEADER_LENGTH);

	int found_at_index;
	RETURN_IF_ERROR(LookUpInSniffTable(m_octets, length, FALSE, TRUE, TRUE, VIDEO, found_at_index));
	if (found_at_index >= 0)
	{
		ContentDetectorPatternData pd = GetPatternData(found_at_index);
		url_type = pd.content_type;
		mime_type = GetMimeType(found_at_index);
	}
	else if (HasMP4Signature(m_octets, length))
	{
		url_type = URL_MEDIA_CONTENT;
		mime_type = "video/mp4";
	}
	m_deterministic = m_length >= DETERMINISTIC_HEADER_LENGTH;
	return OpStatus::OK;
}

#undef CONST_KEYWORD_ARRAY
#undef CONST_KEYWORD_ENTRY
#undef CONST_KEYWORD_END
#define CONST_KEYWORD_ARRAY(name) PREFIX_CONST_ARRAY(static, ContentDetectorPatternData, name, url)
#define CONST_KEYWORD_ENTRY(a,b,c,d) CONST_QUAD_ENTRY(length, a, scriptable, b, content_type, c, simple_type, d)
#define CONST_KEYWORD_END(name) CONST_END(name)

CONST_KEYWORD_ARRAY(PatternData)
	CONST_KEYWORD_ENTRY(16, SCRIPTABLE, URL_HTML_CONTENT, HTML)
	CONST_KEYWORD_ENTRY(7, SCRIPTABLE, URL_HTML_CONTENT, HTML)
	CONST_KEYWORD_ENTRY(7, SCRIPTABLE, URL_HTML_CONTENT, HTML)
	CONST_KEYWORD_ENTRY(9, SCRIPTABLE, URL_HTML_CONTENT, HTML)
	CONST_KEYWORD_ENTRY(9, SCRIPTABLE, URL_HTML_CONTENT, HTML)
	CONST_KEYWORD_ENTRY(5, SCRIPTABLE, URL_HTML_CONTENT, HTML)
	CONST_KEYWORD_ENTRY(6, SCRIPTABLE, URL_HTML_CONTENT, HTML)
	CONST_KEYWORD_ENTRY(7, SCRIPTABLE, URL_HTML_CONTENT, HTML)
	CONST_KEYWORD_ENTRY(8, SCRIPTABLE, URL_HTML_CONTENT, HTML)
	CONST_KEYWORD_ENTRY(4, SCRIPTABLE, URL_HTML_CONTENT, HTML)
	CONST_KEYWORD_ENTRY(8, SCRIPTABLE, URL_HTML_CONTENT, HTML)
	CONST_KEYWORD_ENTRY(8, SCRIPTABLE, URL_HTML_CONTENT, HTML)
	CONST_KEYWORD_ENTRY(4, SCRIPTABLE, URL_HTML_CONTENT, HTML)
	CONST_KEYWORD_ENTRY(7, SCRIPTABLE, URL_HTML_CONTENT, HTML)
	CONST_KEYWORD_ENTRY(5, SCRIPTABLE, URL_HTML_CONTENT, HTML)
	CONST_KEYWORD_ENTRY(4, SCRIPTABLE, URL_HTML_CONTENT, HTML)
	CONST_KEYWORD_ENTRY(6, SCRIPTABLE, URL_HTML_CONTENT, HTML)
	CONST_KEYWORD_ENTRY(6, SCRIPTABLE, URL_XML_CONTENT, XML)
	CONST_KEYWORD_ENTRY(5, SCRIPTABLE, URL_UNKNOWN_CONTENT, UNKNOWN)
	CONST_KEYWORD_ENTRY(11, SAFE, URL_UNKNOWN_CONTENT, UNKNOWN)
	CONST_KEYWORD_ENTRY(4,  NA, URL_TEXT_CONTENT, TEXT)
	CONST_KEYWORD_ENTRY(4,  NA, URL_TEXT_CONTENT, TEXT)
	CONST_KEYWORD_ENTRY(4,  NA, URL_TEXT_CONTENT, TEXT)
	CONST_KEYWORD_ENTRY(6,  SAFE, URL_GIF_CONTENT,  IMAGE)
	CONST_KEYWORD_ENTRY(6,  SAFE, URL_GIF_CONTENT,  IMAGE)
	CONST_KEYWORD_ENTRY(8,  SAFE, URL_PNG_CONTENT,  IMAGE)
	CONST_KEYWORD_ENTRY(3,  SAFE, URL_JPG_CONTENT,  IMAGE)
	CONST_KEYWORD_ENTRY(2,  SAFE, URL_BMP_CONTENT,  IMAGE)
	CONST_KEYWORD_ENTRY(14, SAFE, URL_WEBP_CONTENT, IMAGE)
	CONST_KEYWORD_ENTRY(4,  SAFE, URL_UNKNOWN_CONTENT, UNKNOWN)
	CONST_KEYWORD_ENTRY(5,  SAFE, URL_MEDIA_CONTENT, VIDEO)
	CONST_KEYWORD_ENTRY(12, SAFE, URL_MEDIA_CONTENT, VIDEO)
	CONST_KEYWORD_ENTRY(4,  SAFE, URL_MEDIA_CONTENT, VIDEO)
	CONST_KEYWORD_ENTRY(7,  SAFE, URL_UNKNOWN_CONTENT, UNKNOWN)
	CONST_KEYWORD_ENTRY(4,  SAFE, URL_UNKNOWN_CONTENT, UNKNOWN)
	CONST_KEYWORD_ENTRY(3,  SAFE, URL_UNKNOWN_CONTENT, UNKNOWN)
	// Opera extensions below
#ifdef EXTENDED_MIMETYPES
	CONST_KEYWORD_ENTRY(3, SCRIPTABLE, URL_FLASH_CONTENT, UNKNOWN)
#endif
#ifdef _BITTORRENT_SUPPORT_
	CONST_KEYWORD_ENTRY(11, SAFE, URL_P2P_BITTORRENT, UNKNOWN)
#endif
CONST_KEYWORD_END(PatternData)

ContentDetectorPatternData ContentDetector::GetPatternData(int index)
{
	OP_ASSERT(index >= 0 && index < NUMBER_OF_MIMETYPES);

	return g_PatternData[index];
}

void ContentDetector::GetPattern(int index, unsigned char (&pattern)[PATTERN_MAX_LENGTH])
{
	OP_ASSERT(index >= 0 && index < NUMBER_OF_MIMETYPES);

	const unsigned char patterns[NUMBER_OF_MIMETYPES][PATTERN_MAX_LENGTH] =
	{
		{WS,   0x3C, 0x21, 0x44, 0x4F, 0x43, 0x54, 0x59, 0x50, 0x45, 0x20, 0x48, 0x54, 0x4D, 0x4C, SB } , // <!DOCTYPE HTML
		{WS,   0x3C, 0x48, 0x54, 0x4D, 0x4C, SB  , 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // <HTML
		{WS,   0x3C, 0x48, 0x45, 0x41, 0x44, SB  , 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // <HEAD
		{WS,   0x3C, 0x53, 0x43, 0x52, 0x49, 0x50, 0x54, SB  , 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // <SCRIPT
		{WS,   0x3C, 0x49, 0x46, 0x52, 0x41, 0x4d, 0x45, SB  , 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // <IFRAME
		{WS,   0x3C, 0x48, 0x31, SB  , 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // <H1
		{WS,   0x3C, 0x44, 0x49, 0x56, SB  , 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // <DIV
		{WS,   0x3C, 0x46, 0x4f, 0x4e, 0x54, SB  , 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // <FONT
		{WS,   0x3C, 0x54, 0x41, 0x42, 0x4c, 0x45, SB  , 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // <TABLE
		{WS,   0x3C, 0x41, SB  , 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // <A
		{WS,   0x3C, 0x53, 0x54, 0x59, 0x4c, 0x45, SB  , 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // <STYLE
		{WS,   0x3C, 0x54, 0x49, 0x54, 0x4c, 0x45, SB  , 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // <TITLE
		{WS,   0x3C, 0x42, SB  , 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // <B
		{WS,   0x3C, 0x42, 0x4f, 0x44, 0x59, SB  , 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // <BODY
		{WS,   0x3C, 0x42, 0x52, SB  , 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // <BR
		{WS,   0x3C, 0x50, SB  , 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // <P
		{WS,   0x3C, 0x21, 0x2d, 0x2d, SB  , 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // <!--
		{WS,   0x3C, 0x3f, 0x78, 0x6d, 0x6c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // <?xml (Note the case sensitivity and lack of trailing _>)
		{0x25, 0x50, 0x44, 0x46, 0x2D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // The string "%PDF-", the PDF signature.
		{0x25, 0x21, 0x50, 0x53, 0x2D, 0x41, 0x64, 0x6F, 0x62, 0x65, 0x2D, 0x00, 0x00, 0x00, 0x00, 0x00}, // The string "%!PS-Adobe-", the PostScript signature.
		{0xFE, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // UTF-16BE BOM
		{0xFF, 0xFE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // UTF-16LE BOM
		{0xEF, 0xBB, 0xBF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // UTF-8 BOM
		{0x47, 0x49, 0x46, 0x38, 0x37, 0x61, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // The string "GIF87a", a GIF signature.
		{0x47, 0x49, 0x46, 0x38, 0x39, 0x61, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // The string "GIF89a", a GIF signature.
		{0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // The PNG signature.
		{0xFF, 0xD8, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // A JPEG SOI marker followed by a octet of another marker.
		{0x42, 0x4D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // The string "BM", a BMP signature.
		{0x52, 0x49, 0x46, 0x46, 0x00, 0x00, 0x00, 0x00, 0x57, 0x45, 0x42, 0x50, 0x56, 0x50, 0x00, 0x00}, // The string "RIFF", followed by four bytes, followed by "WEBPVP", a webp signature
		{0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // A Windows Icon signature.
		{0x4F, 0x67, 0x67, 0x53, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // An Ogg audio or video signature.
		{0x52, 0x49, 0x46, 0x46, 0x00, 0x00, 0x00, 0x00, 0x57, 0x41, 0x56, 0x45, 0x00, 0x00, 0x00, 0x00}, // "RIFF" followed by four bytes, followed by "WAVE".
		{0x1A, 0x45, 0xDF, 0xA3, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // The WebM signature [TODO: Use more octets?]
		{0x52, 0x61, 0x72, 0x20, 0x1A, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // A RAR archive.
		{0x50, 0x4B, 0x03, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // A ZIP archive.
		{0x1F, 0x8B, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // A GZIP archive.
		// Opera extensions below
#ifdef EXTENDED_MIMETYPES
		{0x43, 0x57, 0x53, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // Flash content
#endif
#ifdef _BITTORRENT_SUPPORT_
		{0x64, 0x38, 0x3A, 0x61, 0x6E, 0x6E, 0x6F, 0x75, 0x6E, 0x63, 0x65, 0x00, 0x00, 0x00, 0x00, 0x00}, // Torrent file
#endif
	};

	op_memcpy(pattern, patterns[index], PATTERN_MAX_LENGTH);
}

void ContentDetector::GetMask(int index, unsigned char (&mask)[PATTERN_MAX_LENGTH])
{
	OP_ASSERT(index >= 0 && index < NUMBER_OF_MIMETYPES);

	const unsigned char masks[NUMBER_OF_MIMETYPES][PATTERN_MAX_LENGTH] =
	{
		{0xFF, 0xFF, 0xFF, 0xDF, 0xDF, 0xDF, 0xDF, 0xDF, 0xDF, 0xDF, 0xFF, 0xDF, 0xDF, 0xDF, 0xDF, 0xFF}, // <!DOCTYPE HTML
		{0xFF, 0xFF, 0xDF, 0xDF, 0xDF, 0xDF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // <HTML
		{0xFF, 0xFF, 0xDF, 0xDF, 0xDF, 0xDF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // <HEAD
		{0xFF, 0xFF, 0xDF, 0xDF, 0xDF, 0xDF, 0xDF, 0xDF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // <SCRIPT
		{0xFF, 0xFF, 0xDF, 0xDF, 0xDF, 0xDF, 0xDF, 0xDF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // <IFRAME
		{0xFF, 0xFF, 0xDF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // <H1
		{0xFF, 0xFF, 0xDF, 0xDF, 0xDF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // <DIV
		{0xFF, 0xFF, 0xDF, 0xDF, 0xDF, 0xDF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // <FONT
		{0xFF, 0xFF, 0xDF, 0xDF, 0xDF, 0xDF, 0xDF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // <TABLE
		{0xFF, 0xFF, 0xDF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // <A
		{0xFF, 0xFF, 0xDF, 0xDF, 0xDF, 0xDF, 0xDF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // <STYLE
		{0xFF, 0xFF, 0xDF, 0xDF, 0xDF, 0xDF, 0xDF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // <TITLE
		{0xFF, 0xFF, 0xDF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // <B
		{0xFF, 0xFF, 0xDF, 0xDF, 0xDF, 0xDF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // <BODY
		{0xFF, 0xFF, 0xDF, 0xDF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // <BR
		{0xFF, 0xFF, 0xDF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // <P
		{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // <!--
		{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // <?xml (Note the case sensitivity and lack of trailing _>)
		{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // The string "%PDF-", the PDF signature.
		{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00}, // The string "%!PS-Adobe-", the PostScript signature.
		{0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // UTF-16BE BOM
		{0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // UTF-16LE BOM
		{0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // UTF-8 BOM
		{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // The string "GIF87a", a GIF signature.
		{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // The string "GIF89a", a GIF signature.
		{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // The PNG signature.
		{0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // A JPEG SOI marker followed by a octet of another marker.
		{0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // The string "BM", a BMP signature.
		{0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00}, // The string "RIFF", followed by four bytes, followed by "WEBPVP", a webp signature
		{0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // A Windows Icon signature.
		{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // An Ogg audio or video signature.
		{0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00}, // "RIFF" followed by four bytes, followed by "WAVE".
		{0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // The WebM signature [TODO: Use more octets?]
		{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // A RAR archive.
		{0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // A ZIP archive.
		{0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // A GZIP archive.
		// Opera extensions below
#ifdef EXTENDED_MIMETYPES
		{0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // Flash content
#endif
#ifdef _BITTORRENT_SUPPORT_
		{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00}, // Torrent file
#endif
	};

	op_memcpy(mask, masks[index], PATTERN_MAX_LENGTH);

}

PREFIX_CONST_ARRAY_PRESIZED(const char*, mime_types, url)
	CONST_ENTRY("text/html")
	CONST_ENTRY("text/html")
	CONST_ENTRY("text/html")
	CONST_ENTRY("text/html")
	CONST_ENTRY("text/html")
	CONST_ENTRY("text/html")
	CONST_ENTRY("text/html")
	CONST_ENTRY("text/html")
	CONST_ENTRY("text/html")
	CONST_ENTRY("text/html")
	CONST_ENTRY("text/html")
	CONST_ENTRY("text/html")
	CONST_ENTRY("text/html")
	CONST_ENTRY("text/html")
	CONST_ENTRY("text/html")
	CONST_ENTRY("text/html")
	CONST_ENTRY("text/html")
	CONST_ENTRY("text/xml")
	CONST_ENTRY("application/pdf")
	CONST_ENTRY("application/postscript")
	CONST_ENTRY("text/plain")
	CONST_ENTRY("text/plain")
	CONST_ENTRY("text/plain")
	CONST_ENTRY("image/gif")
	CONST_ENTRY("image/gif")
	CONST_ENTRY("image/png")
	CONST_ENTRY("image/jpeg")
	CONST_ENTRY("image/bmp")
	CONST_ENTRY("image/webp")
	CONST_ENTRY("image/vnd.microsoft.icon")
	CONST_ENTRY("application/ogg")
	CONST_ENTRY("audio/wave")
	CONST_ENTRY("video/webm")
	CONST_ENTRY("application/x-rar-compressed")
	CONST_ENTRY("application/zip")
	CONST_ENTRY("application/x-gzip")
	// Opera extensions below
#ifdef EXTENDED_MIMETYPES
	CONST_ENTRY("application/x-shockwave-flash")
#endif
#ifdef _BITTORRENT_SUPPORT_
	CONST_ENTRY("application/x-bittorrent")
#endif
CONST_END(mime_types)

const char *ContentDetector::GetMimeType(int index)
{
	return g_mime_types[index];
}

OP_STATUS ContentDetector::LookUpInSniffTable(const char *octets, unsigned long octets_length, BOOL must_be_non_scriptable, BOOL check_with_mask,
											  BOOL specific_type_only, ContentDetectorSimpleType specific_type, int &found_at_index)
{
	found_at_index = -1;
	unsigned char pattern[PATTERN_MAX_LENGTH];
	unsigned char mask[PATTERN_MAX_LENGTH];

	for (int index = 0; index < NUMBER_OF_MIMETYPES; index++)
	{
		if (must_be_non_scriptable && GetPatternData(index).scriptable != SAFE)
			continue;

		if (specific_type_only && GetPatternData(index).simple_type != specific_type)
			continue;

		unsigned int octet_index = 0;
		unsigned int pattern_index = 0;

		GetPattern(index, pattern);
		unsigned int pattern_length = GetPatternData(index).length;

		if (octets_length < pattern_length)
			continue;

		// if needed, check for arbitrary amount of white spaces.
		if (pattern[0] == WS)
		{
			while (octet_index < octets_length)
			{
				char c = octets[octet_index];
				if (!op_isspace(c) || c == 0x0B) // vertical tabs (0x0B) not included.
				{
					break;
				}
				octet_index++;
			}

			if (octet_index == octets_length)
				continue;

			pattern_index++;
		}

		BOOL check_if_octets_terminate_with_space_or_bracket = FALSE;
		char last_octet = 0; // dummy value, not used if check_if_octets_terminate_with_space_or_bracket == FALSE
		if (pattern[pattern_length - 1] == SB)
		{
			last_octet = octets[pattern_length - 1 - pattern_index + octet_index];
			pattern_length--;
			check_if_octets_terminate_with_space_or_bracket = TRUE;
		}

		unsigned potential_octet_match_length = MIN(octets_length - octet_index, pattern_length - pattern_index);

		if (check_with_mask)
		{
			GetMask(index, mask);
			BOOL match = TRUE;
			for (unsigned i = 0; i < potential_octet_match_length && match; ++i)
				match = (octets[octet_index+i] & mask[pattern_index+i]) == pattern[pattern_index+i];
			if (!match)
				continue;
		}
		else if (op_memcmp(octets + octet_index, pattern + pattern_index, potential_octet_match_length) != 0)
			continue;

		if (check_if_octets_terminate_with_space_or_bracket && last_octet != 0x20 && last_octet != 0x3E)
			continue;

		found_at_index = index;
		break;
	}

	return OpStatus::OK;
}
