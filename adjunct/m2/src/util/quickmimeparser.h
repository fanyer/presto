/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef QUICK_MIME_PARSER_H
#define QUICK_MIME_PARSER_H

#include "adjunct/m2/src/engine/message.h"

/** @brief A quick MIME parser used to get text parts and attachments out of mime-encoded content
  * This is a class for decoding a MIME body that can be used from a thread, used by the indexing parts of M2.
  * The mime module should eventually takeover this functionality, since this implementation
  * is basically duplicate functionality, and doesn't benefit from bugs fixed in the mime module.
  */
class QuickMimeParser
{
public:
	struct Attachment
	{
		Attachment() : media_type(Message::TYPE_UNKNOWN), media_subtype(Message::SUBTYPE_UNKNOWN) {}

		Message::MediaType media_type;
		Message::MediaSubtype media_subtype;
	};

	/** Gets all text parts out of a MIME body, and saves the text in 'output'
	  * @param content_type Content-Type of the given MIME body
	  * @param content_transfer_encoding Content-Transfer-Encoding of the given MIME body (can be empty)
	  * @param content_disposition Content-Disposition of the given MIME body (can be empty)
	  * @param part The MIME body part to decode
	  * @param output Text found in 'part'
	  */
	OP_STATUS GetTextParts(const OpStringC8& content_type,
						   const OpStringC8& content_transfer_encoding,
						   const OpStringC8& content_disposition,
						   char* part,
						   OpString& output) const;


	/** Gets the types of attachments
	  * @param content_type Content-Type of the given MIME body
	  * @param content_disposition Content-Disposition of the given MIME body (can be empty)
	  * @param part The MIME body part to decode
	  * @param attachments Output: Attachment types for this MIME body
	  */
	OP_STATUS GetAttachmentType(const OpStringC8& content_type,
								const OpStringC8& content_disposition,
								char* part,
								OpAutoVector<Attachment>& attachments) const;


	/** Gets an attribute from a Content-Type: header
	  * @param content_type The Content-Type header
	  * @param attribute Attribute to check for (example: "charset")
	  * @param value Output value
	  */
	OP_STATUS GetAttributeFromContentType(const OpStringC8& content_type,
										  const OpStringC8& attribute,
										  OpString8& value) const;

	/** Gets the value for a specifific MIME header from a multipart MIME body
	  * @param multipart Valid multipart MIME body, first header assumed to start with \r\n
	  * @param header_name Name of header to find
	  * @param value Output value of header
	  */
	OP_STATUS GetHeaderValueFromMultipart(const OpStringC8& multipart,
										  const OpStringC8& header_name,
										  OpString8& value) const;

private:
	const char* FindLineEnding(const char* string) const;
	const char* SkipLineEnding(const char* string) const;
	
	/** Gets the types of attachments
	  */
	OP_STATUS GetAttachmentType(const OpStringC8& content_type,
								const OpStringC8& content_disposition,
								char* part,
								OpAutoVector<Attachment>& attachments,
								BOOL& first) const;

	/** Tries to find the body in a multipart MIME body
	  */
	char*	  GetBodyFromMultipart(char* multipart,
								   const char* boundary,
								   int& body_length,
								   OpString8& content_type,
								   OpString8& content_transfer_encoding,
								   OpString8& content_disposition) const;

	/** Gets the attachment type from a content-type
	  */
	void	  GetAttachmentTypeFromContentType(const OpStringC8& content_type,
											   Attachment& attachment) const;

	OP_STATUS GetTextParts(const OpStringC8& content_type,
						   const OpStringC8& content_transfer_encoding,
						   const OpStringC8& content_disposition,
						   char* part,
						   OpString& output,
						   unsigned depth,
						   BOOL found_plain_text) const;

	/** Remove HTML tags, keeping only what looks like text
	  * @param txt Text that should be adapted
	  * @param txt_len Length of text in txt
	  * @return The new length of the string.
	  */
	int RemoveHTMLTags(uni_char* txt) const;

	/**
	 * Replace entity and character references
	 *
	 * It replaces entity references regardless of whether they are terminated by
	 * a semicolon or not (as dictated by HTML and SGML) and also replaces both
	 * decimal and hexadecimal character references. If require_termination is
	 * true only entity references explicitly terminated with ';' are replaced.
	 * This is done to avoid replacing server-side script parameters.
	 *
	 * @param txt Text string to replace entity references in.
	 * @return The new length of the string.
	 *
	 * See HTML 4.0 spec "5.3 Character References".
	 * This is a copy of the ReplaceEscapes function in logdoc/logdoc_util.h,
	 * the only difference is that this function is thread-safe.
	 */
	int ReplaceEscapes(uni_char *txt) const;
	
	/**
	 * Look up an HTML character entity reference by name.
	 *
	 * @param esc_seq The character entity (without leading ampersand) to look up.
	 * @param len Length of character entity string, in uni_chars.
	 * @return the referenced character.
	 */
	uni_char GetEscapeChar(const uni_char* esc_seq, int len) const;
};

#endif // QUICK_MIME_PARSER_H
