/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef MESSAGE_ENCODER_H
#define MESSAGE_ENCODER_H

#include "adjunct/m2/src/engine/header.h"
#include "modules/upload/upload.h"
#include "adjunct/m2/src/util/quote.h"

class MessageEncoder
{
public:

	/** Default constructor
	  */
	MessageEncoder()
		: m_allow_8bit(FALSE){}

	/** Add a header that should appear in the final encoded message
	  * @param type Which header to add
	  * @param value Value of header to add
	  * @param charset Charset to encode header value with
	  */
	OP_STATUS AddHeaderValue(Header::HeaderType type, const OpStringC& value, const OpStringC8& charset);

	/** Add a header that should appear in the final encoded message (used for Header::UNKNOWN type headers)
	  * @param name Name of the header to add
	  * @param value Value of header to add
	  * @param charset Charset to encode header value with
	  */
	OP_STATUS AddHeaderValue(const OpStringC8& name, const OpStringC& value, const OpStringC8& charset);

	/** Set an HTML part for this message
	  * @param html_body Part to use as HTML part - this function will take ownership
	  */
	OP_STATUS SetHTMLBody(OpAutoPtr<Upload_Base> html_body);

	/** Set a wrapper to use for wrapping plain text
	  * @param wrapper Wrapper to use - this encoder will take ownership
	  */
	void SetTextWrapper(OpAutoPtr<OpQuote> wrapper) { m_wrapper.reset(wrapper.release()); }

	/** Set a plain text part for this message
	  * @param plain_text Unencoded text for message
	  * @param charset Charset to encode plain_text with
	  * @param quote object to use, MessageEncoder will delete it when not needed anymore
	  */
	OP_STATUS SetPlainTextBody(const OpStringC& plain_text, const OpStringC8& charset);

	/** Add an attachment for this message
	  * @param attachment_path Full path to attachment file
	  * @param suggested_filename Attachment filename that we should use (to avoid using the cache filename)
	  * @param charset Charset to encode attachment with (if applicable)
	  * @param mime_type File MIME type
	  */
	virtual OP_STATUS AddAttachment(const OpStringC& attachment_path, const OpStringC& suggested_filename, const OpStringC8& charset, const OpStringC8& mime_type);

	/** Output an encoded message, object should not be used after invoking this function
	  * @param encoded_message Where to output encoded message
	  * @param encode_attachments Whether to encode the attachments (used for sending) or just link to them with our own headers (used for drafts)
	  */
	OP_STATUS Encode(OpString8& encoded_message, const OpStringC8& charset, BOOL encode_attachments = TRUE);

	/** Set whether the message should allow the use of the 8-bit content transfer encoding or not
	  * @param allow_8bit TRUE if 8-bit should be allowed
	  */
	void SetAllow8BitTransfer(BOOL allow_8bit) { m_allow_8bit = allow_8bit; }

private:
	OP_STATUS InitializeAttachments();
	OP_STATUS CreateMainPart(OpAutoPtr<Upload_Base>& main_part, BOOL encode_attachments = TRUE);
	OP_STATUS CreateTextPart(OpAutoPtr<Upload_Base>& text_part);
	OP_STATUS CreateMultipartAlternative(OpAutoPtr<Upload_Base>& text_part);
	OP_STATUS CreateMultipartWithAttachments(OpAutoPtr<Upload_Base> text_part, OpAutoPtr<Upload_Base>& target);
	OP_STATUS AddHeadersTo(Upload_Base& main_part);
	OP_STATUS EncodeMainPartToMessage(Upload_Base& main_part, OpString8& encoded_message, const OpStringC8& charset);

	OpAutoPtr<Upload_Multipart> m_attachments;
	OpAutoPtr<Upload_Base>  m_plain_text_body;
	OpAutoPtr<Upload_Base>  m_html_body;
	OpAutoPtr<OpQuote>		m_wrapper;
	AutoDeleteHead			m_header_list;
	BOOL					m_allow_8bit;
};

#endif // MESSAGE_ENCODER_H
