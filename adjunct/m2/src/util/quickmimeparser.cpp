/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#ifdef M2_SUPPORT

#include "adjunct/m2/src/util/quickmimeparser.h"

#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/util/qp.h"
#include "modules/logdoc/data/entities_len.h"
#include "modules/logdoc/data/entities.inl"

namespace QuickMimeParserConstants
{
	const unsigned MaxDepth = 30;	///< Max. nesting of multiparts
	const unsigned MaxParts = 100;	///< Max. parts in a multipart body
};


/***********************************************************************************
 ** Gets all text parts out of a MIME body, and saves the text in 'output'
 **
 ** QuickMimeParser::GetTextParts
 ***********************************************************************************/
OP_STATUS QuickMimeParser::GetTextParts(const OpStringC8& content_type,
										const OpStringC8& content_transfer_encoding,
										const OpStringC8& content_disposition,
										char*			  part,
										OpString&		  output) const
{
	return GetTextParts(content_type,
						content_transfer_encoding,
						content_disposition,
						part,
						output,
						0,
					    FALSE);
}


/***********************************************************************************
 ** Gets all text parts out of a MIME body, and saves the text in 'output'
 **
 ** QuickMimeParser::GetTextParts
 ***********************************************************************************/
OP_STATUS QuickMimeParser::GetTextParts(const OpStringC8& content_type,
										const OpStringC8& content_transfer_encoding,
										const OpStringC8& content_disposition,
										char*			  part,
										OpString&		  output,
									    unsigned		  depth,
									    BOOL			  found_plain_text) const
{
	if (depth > QuickMimeParserConstants::MaxDepth)
		return OpStatus::OK;

	OP_STATUS ret = OpStatus::OK;

	// We only handle text and multipart parts
	if ((content_disposition.IsEmpty() || !content_disposition.CompareI("inline",     6)) &&
		(content_type       .IsEmpty() || !content_type       .CompareI("text/plain", 10) ||
		 (!found_plain_text            && !content_type       .CompareI("text/html",  9))))
	{
		// Check for uuencoded texts in the message, recognizable by the line "begin [0-9]+ [^\r\n]+\r\n"
		char* uuencode_start = op_strstr(part, "begin ");

		while (uuencode_start)
		{
			// check for uuencode line
			char lineend[2];
			if (op_sscanf(uuencode_start + 6, "%*1o%*1o%*1o %*255[^\r\n]%2c", lineend) > 0 &&
				!op_strncmp(lineend, "\r\n", 2))
			{
				// uuencode_start is a valid uuencode line
				break;
			}

			uuencode_start = op_strstr(uuencode_start + 6, "begin ");
		}

		// Limit where uuencoded string starts
		if (uuencode_start)
			*uuencode_start = '\0';

		// Text part, decode and add to output
		OpString8	charset;
		BOOL	  	error_found, warning_found;
		const BYTE* byte_part = reinterpret_cast<BYTE*>(part);

		// Get charset
		if (OpStatus::IsError(GetAttributeFromContentType(content_type, "charset", charset)))
			charset.Set("us-ascii"); // if not defined, use us-ascii, in line with RFC2045

		found_plain_text = content_type.CompareI("text/html", 9) != 0;
		int len_before_append = found_plain_text ? 0 : output.Length();

		// Decode
		if (!content_transfer_encoding.CompareI("quoted-printable", 16))
		{
			ret = OpQP::QPDecode(byte_part, charset, output, warning_found, error_found, FALSE);
		}
		else if (!content_transfer_encoding.CompareI("base64", 6))
		{
			ret = OpQP::Base64Decode(byte_part, charset, output, warning_found, error_found, FALSE);
		}
		else
		{
			ret = g_m2_engine->GetInputConverter().ConvertToChar16(charset, part, output, TRUE, TRUE);
		}

		// Convert HTML entities if necessary
		if (!found_plain_text)
		{
			RemoveHTMLTags(output.CStr() + len_before_append);
			ReplaceEscapes(output.CStr() + len_before_append);
		}

		// Put back uuencode character
		if (uuencode_start)
			*uuencode_start = 'b';
	}
	else if (!content_type.CompareI("multipart", 9))
	{
		OpString8 boundary;
		OpString8 part_content_type, part_encoding, part_disposition;
		int		  body_length;

		// Multipart, loop through all parts
		if (OpStatus::IsError(GetAttributeFromContentType(content_type, "boundary", boundary)))
			return OpStatus::OK;

		for (unsigned i = 0;
			 i < QuickMimeParserConstants::MaxParts &&
					 (part = GetBodyFromMultipart(part, boundary.CStr(), body_length, part_content_type, part_encoding, part_disposition)) != NULL;
			 i++)
		{
			char hold = part[body_length];
			part[body_length] = '\0';

			RETURN_IF_ERROR(GetTextParts(part_content_type, part_encoding, part_disposition, part, output, depth + 1, found_plain_text));

			part[body_length] = hold;
			part += body_length;
		}
	}

	return ret;
}


/***********************************************************************************
 ** Gets the attachment type of the first attachment
 **
 ** QuickMimeParser::GetAttachmentList
 ***********************************************************************************/
OP_STATUS QuickMimeParser::GetAttachmentType(const OpStringC8&		   content_type,
											 const OpStringC8&		   content_disposition,
											 char*					   part,
											 OpAutoVector<Attachment>& attachments) const
{
	BOOL first = TRUE;
	return GetAttachmentType(content_type, content_disposition, part, attachments, first);
}


/***********************************************************************************
 ** Gets the attachment type of the first attachment
 **
 ** QuickMimeParser::GetAttachmentList
 ***********************************************************************************/
OP_STATUS QuickMimeParser::GetAttachmentType(const OpStringC8&		   content_type,
											 const OpStringC8&		   content_disposition,
											 char*					   part,
											 OpAutoVector<Attachment>& attachments,
											 BOOL&					   first) const
{
	// If this is the first call, and it's a text/plain or text/html part,
	// we assume this is the mail body and it doesn't count
	if (first && (content_type.IsEmpty() || !content_type.CompareI("text/plain", 10) || !content_type.CompareI("text/html", 9)))
	{
		first = FALSE;
		return OpStatus::OK;
	}

	// Inline parts are not attachments
	if (!content_disposition.CompareI("inline", 6))
	{
		first = FALSE;
		return OpStatus::OK;
	}

	// Dig deeper for multiparts
	if (!content_type.CompareI("multipart", 9))
	{
		OpString8 boundary;
		OpString8 part_content_type, part_encoding, part_disposition;
		int		  body_length;
		BOOL	  alternative = !content_type.CompareI("multipart/alternative", 21);

		// Multipart, loop through all parts
		RETURN_IF_ERROR(GetAttributeFromContentType(content_type, "boundary", boundary));

		while ((part = GetBodyFromMultipart(part, boundary.CStr(), body_length, part_content_type, part_encoding, part_disposition)) != 0)
		{
			char hold = part[body_length];
			part[body_length] = '\0';

			RETURN_IF_ERROR(GetAttachmentType(part_content_type, part_disposition, part, attachments, first));

			part[body_length] = hold;

			// For multipart/alternative, we only consider the first part - else proceed to next part
			if (alternative)
				break;

			part += body_length;
		}

		return OpStatus::OK;
	}

	// Not a multipart, analyze type
	OpAutoPtr<Attachment> attachment (OP_NEW(Attachment, ()));
	if (!attachment.get())
		return OpStatus::ERR_NO_MEMORY;

	GetAttachmentTypeFromContentType(content_type, *attachment.get());

	// Add to output
	RETURN_IF_ERROR(attachments.Add(attachment.get()));
	attachment.release();

	return OpStatus::OK;
}


/***********************************************************************************
 ** Gets an attribute from a Content-Type: header
 **
 ** QuickMimeParser::GetAttributeFromContentType
 ***********************************************************************************/
OP_STATUS QuickMimeParser::GetAttributeFromContentType(const OpStringC8& content_type,
													   const OpStringC8& attribute,
													   OpString8& value) const
{
	// Preparation
	if (content_type.IsEmpty() || attribute.IsEmpty())
		return OpStatus::ERR;

	// Find the attribute
	const char* att_ptr = op_stristr(content_type.CStr(), attribute.CStr());
	if (!att_ptr)
		return OpStatus::ERR;

	// Jump past "attribute"
	att_ptr += attribute.Length();

	// Jump past '=' and spaces if necessary
	while (*att_ptr == ' ')
		att_ptr++;

	while (*att_ptr == '=')
		att_ptr++;

	while (*att_ptr == ' ')
		att_ptr++;

	// Read value, quoted or unquoted (see RFC 2045, 5.1 for allowed characters)
	if (!value.Reserve(op_strlen(att_ptr)))
		return OpStatus::ERR_NO_MEMORY;

	if (op_sscanf(att_ptr, "\"%[^\"]\"", value.CStr()) < 1 &&
		op_sscanf(att_ptr, "%[^][ ()<>@,;:\\\"?=]", value.CStr()) < 1)
		return OpStatus::ERR;

	return OpStatus::OK;
}


/***********************************************************************************
 ** Tries to find the main part ('mail body') in a multipart MIME body
 **
 ** QuickMimeParser::GetBodyFromMultipart
 ***********************************************************************************/
char* QuickMimeParser::GetBodyFromMultipart(char*		multipart,
											const char* boundary,
											int&		body_length,
											OpString8&	content_type,
											OpString8&	content_transfer_encoding,
											OpString8&	content_disposition) const
{
	// Format that we expect to find in 'multipart':
	//    --boundary\r\n
	//    header1\r\n
	//    header2\r\n
	//    \r\n
	//    body
	//    --boundary
	//
	// We need to find the Content-Type header (if it exists) and the body.

	OpString8   boundary_delim;

	if (!multipart || !boundary)
		return NULL;

	content_type.Empty();
	if (OpStatus::IsError(boundary_delim.AppendFormat("--%s", boundary)))
		return NULL;

	// Find first boundary
	char* body = op_strstr(multipart, boundary_delim.CStr());
	if (!body)
		return NULL;

	// Go past boundary
	body = op_strstr(body, "\r\n");
	if (!body)
		return NULL;

	// Mark end of headers
	char* headers_end = op_strstr(body, "\r\n\r\n");
	if (!headers_end)
		return NULL;
	headers_end[2] = '\0';

	// Find content-type and content-transfer-encoding header
	GetHeaderValueFromMultipart(body, "content-type", content_type);
	GetHeaderValueFromMultipart(body, "content-transfer-encoding", content_transfer_encoding);
	GetHeaderValueFromMultipart(body, "content-disposition", content_disposition);

	// Go past headers
	headers_end[2] = '\r';
	body = headers_end + 4;

	// Find end boundary
	const char* body_end = op_strstr(body, boundary_delim.CStr());
	if (!body_end)
		return NULL;
	body_length = body_end - body;

	return body;
}


/***********************************************************************************
 ** Gets the value for a specifific MIME header from a multipart MIME body
 **
 ** QuickMimeParser::GetHeaderValueFromMultipart
 ***********************************************************************************/
OP_STATUS QuickMimeParser::GetHeaderValueFromMultipart(const OpStringC8& multipart, const OpStringC8& header_name, OpString8& value) const
{
	value.Empty();

	// First check if header exists
	OpString8 header_find_string;
	RETURN_IF_ERROR(header_find_string.AppendFormat("\n%s: ", header_name.CStr()));

	const char* header_pos = op_stristr(multipart.CStr(), header_find_string.CStr());
	if (!header_pos)
		return OpStatus::ERR;

	// Now handle the value
	header_pos += header_find_string.Length();
	const char* header_end = FindLineEnding(header_pos);
	if (!header_end)
		return OpStatus::ERR;

	RETURN_IF_ERROR(value.Set(header_pos, header_end - header_pos));
	header_end = SkipLineEnding(header_end);

	// Detect folded headers (lines that start with LWSP characters, SP or HTAB)
	while (*header_end == ' ' || *header_end == '\t')
	{
		header_pos = header_end;
		header_end = FindLineEnding(header_pos);
		if (!header_end)
			return OpStatus::ERR;

		RETURN_IF_ERROR(value.Append(header_pos, header_end - header_pos));
		header_end = SkipLineEnding(header_end);
	}

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** QuickMimeParser::FindLineEnding
 ***********************************************************************************/
const char* QuickMimeParser::FindLineEnding(const char* string) const
{
	const char* line_end = op_strchr(string, '\n');
	if (line_end > string && *(line_end - 1) == '\r')
		line_end--;

	return line_end;
}


/***********************************************************************************
 **
 **
 ** QuickMimeParser::SkipLineEnding
 ***********************************************************************************/
const char* QuickMimeParser::SkipLineEnding(const char* string) const
{
	if (*string == '\r')
		string++;
	if (*string == '\n')
		string++;

	return string;
}


/***********************************************************************************
 ** Gets the attachment type from a content-type
 **
 ** QuickMimeParser::GetAttachmentTypeFromContentType
 ***********************************************************************************/
void QuickMimeParser::GetAttachmentTypeFromContentType(const OpStringC8& content_type,
													   Attachment& attachment) const
{
	if (content_type.IsEmpty())
		return;

	// Check for our known types
	if (!content_type.CompareI("APPLICATION", 11))
	{
		attachment.media_type = Message::TYPE_APPLICATION;
		if (content_type[11])
		{
			OpStringC8 subtype_str(content_type.CStr() + 12);

			if (!subtype_str.CompareI("ZIP", 3) ||
				!subtype_str.CompareI("X-ZIP", 5) ||
				!subtype_str.CompareI("X-RAR-COMPRESSED", 16))
			{
				attachment.media_subtype = Message::SUBTYPE_ARCHIVE;
			}
			else if (!subtype_str.CompareI("OGG", 3))
			{
				attachment.media_subtype = Message::SUBTYPE_OGG;
			}
		}
	}
	else if (!content_type.CompareI("AUDIO", 5))
		attachment.media_type = Message::TYPE_AUDIO;
	else if (!content_type.CompareI("IMAGE", 5))
		attachment.media_type = Message::TYPE_IMAGE;
	else if (!content_type.CompareI("VIDEO", 5))
		attachment.media_type = Message::TYPE_VIDEO;
}


/***********************************************************************************
 ** Remove HTML tags, keeping only what looks like text
 **
 ** QuickMimeParser::RemoveHTMLTags
 ** NB: Very rudimentary, only works guaranteed on our self-generated RSS messages
 ***********************************************************************************/
int QuickMimeParser::RemoveHTMLTags(uni_char* txt) const
{
	if (!txt)
		return 0;

	// Check if there is an HTML body
	const uni_char* scan = uni_stristr(txt, UNI_L("<body"));
	const uni_char* end  = scan ? uni_stristr(scan, UNI_L("</body")) : 0;

	if (!scan || !end)
	{
		*txt = '\0';
		return 0;
	}

	// Do replacing
	uni_char* replace = txt;

	while (scan < end)
	{
		// Remove what's inside HTML tags
		const uni_char* closing;

		if (*scan == '<' && (closing = uni_strchr(scan + 1, '>')) != NULL)
			scan = closing + 1;
		else
			*replace++ = *scan++;
	}
	*replace = '\0';

	return replace - txt;
}


/***********************************************************************************
 ** Replace
 **
 ** QuickMimeParser::ReplaceEscapes
 **
 ** This is a copy of the ReplaceEscapes function in logdoc/logdoc_util.h,
 ** the only difference is that this function is thread-safe.
 ***********************************************************************************/
int QuickMimeParser::ReplaceEscapes(uni_char* txt) const
{
	if (!txt)
		return 0;

	// This is the part of the windows-1252 table we actually need for the
	// ReplaceEscapes() function.
	static const unsigned short win1252[160 - 128] =
	{
		8364, NOT_A_CHARACTER, 8218, 402, 8222, 8230, 8224, 8225,
		710, 8240, 352, 8249, 338, NOT_A_CHARACTER, 381, NOT_A_CHARACTER,
		NOT_A_CHARACTER, 8216, 8217, 8220, 8221, 8226, 8211, 8212,
		732, 8482, 353, 8250, 339, NOT_A_CHARACTER, 382, 376
	};

	const uni_char* scan = txt;
	uni_char* replace = txt;

	while (*scan)
	{
		if (*scan == '&')
		{
			unsigned int char_no = 32; // the referenced character number
			const uni_char* esc_end = scan;

			if (*(scan + 1) == '#')
			{
				// Look for numeric reference
				if ((*(scan + 2) == 'x' || *(scan + 2) == 'X') && uni_isxdigit(scan[3]))
				{ // Hexadecimal
					esc_end = scan + 3;

					while (uni_isxdigit(*esc_end))
						esc_end++;

					uni_char* dummy;
					char_no = uni_strtol(scan + 3, &dummy, 16);
				}
				else if (Unicode::IsDigit(scan[2]))
				{ // Decimal
					esc_end = scan + 2;

					while (uni_isdigit(*esc_end))
						esc_end++;

					char_no = uni_atoi(scan + 2);
				}
				else // no number after the &# stuff should not be converted
				{
					*replace++ = *scan++;
					*replace++ = *scan; // skip both characters
					scan += 2; // update the index when scan is updated
					continue;
				}

				if (char_no == 0 || char_no > 0x10FFFF)
				{
					// Nul character or non-Unicode character is replaced
					// with U+FFFD.
					char_no = 0xFFFD;
				}

				if (*esc_end == ';') esc_end++;
			}
			else
			{
				// Look for character entity

				esc_end = scan + 1;

				// Supported entities are made up of alphanumerical
				// characters which are all ASCII.
				while (uni_isalnum(*esc_end) && *esc_end < 128)
					esc_end++;

				char_no = GetEscapeChar(scan + 1, esc_end - scan - 1);

				if (*esc_end == ';')
					esc_end++;
				else if (char_no == '<' || char_no == '>' || char_no == '&' || char_no == '"')
				{
				    // We always accept these because MSIE does, regardless of require_termination
					// setting
				}
				else if (char_no > 255)
				{
					// We're less tolerant against errors with non latin-1 chars regardless
					// of the require_termination setting. This makes us compatible with MSIE.
					char_no = 0; // We required this to be terminated and it is not.
				}
			}


			if (char_no)
				scan = esc_end;

			// Non-breaking space (U+00A0).
			if (char_no == 160)
				char_no = ' ';

            // encode and insert character
			if (char_no >= 128 && char_no < 160)
			{
				// Treat numerical entities in the [128,160) range as
				// Windows-1252. Although not according to the standard,
				// this is what is used today.
				*replace++ = win1252[char_no - 0x80];
			}
			else if (char_no > 0 && char_no < 0x10000)
			{
				*replace++ = char_no;
			}
			else if (char_no >= 0x10000)
			{
				// a character outside the BMP was referenced, so we must
				// encode it using surrogates (grrrmph!) the procedure is
				// described in RFC 2781.
				char_no -= 0x10000;
				*replace++ = 0xD800 | (char_no >> 10);
				*replace++ = 0xDC00 | (char_no & 0x03FF);
			}
			else
			{
				*replace++ = *scan++;
			}
		} // if(*scan == '&')
		else
		{
			*replace++ = *scan++;
		}
	} // while(*scan)
	*replace = 0;

	return replace - txt;
}


/***********************************************************************************
 ** QuickMimeParser::GetEscapeChar
 **
 ** This is a copy of the GetEscapeChar function in logdoc/logdoc_util.cpp
 ***********************************************************************************/
uni_char QuickMimeParser::GetEscapeChar(const uni_char* esc_seq, int len) const
{
	if (len <= AMP_ESC_MAX_SIZE)
	{
		const char * const *end = AMP_ESC + AMP_ESC_idx[len + 1];
		for (const char * const *p = AMP_ESC + AMP_ESC_idx[len]; p < end; ++ p)
		{
			int n = 0;
			while (n < len && (*p)[n] == esc_seq[n])
				n++;
			if (n == len)
				return AMP_ESC_char[p - AMP_ESC];
		}
	}

	return 0;
}


#endif // M2_SUPPORT
