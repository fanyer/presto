/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef _MIMEUTIL_H_
#define _MIMEUTIL_H_


#ifdef _MIME_SUPPORT_

#include "modules/url/url_id.h"
#include "modules/formats/hdsplit.h"

class CharConverter;
class URL;

/**
 * Convert an email header string into unicode
 * @param target converted and decoded unicode string
 * @param source header string in arbitrary charset and/or encoding
 * @param maxlen maximum length od source to convert
 * @param toplevel detect a charset and convert from it
 * @param structured source can contain several levels marked by (), which will be treated recursively
 * @param def_charset default charset of a document, if charset cannot be detected
 * @param converter2 charset converter to be used if not toplevel
 * @param contain_email_address don't break email address formating
 */
void RemoveHeaderEscapes(OpString &target,const char *&source,int maxlen,
						  BOOL toplevel=TRUE,BOOL structured=TRUE, 
						  const char *def_charset=NULL, CharConverter *converter2=NULL, BOOL contain_email_address= FALSE);

/**
 * Create an attachment URL
 * @param base_url_type one of URL_ATTACHMENT, URL_NEWSATTACHMENT or URL_SNEWSATTACHMENT
 * @param no_store do not store the created URL in cache
 * @param content_id if not NULL, construct a cid: URL into content_id_url
 * @param ext0 possible extension of suggested_filename, can be empty
 * @param content_type mime type of the attachment
 * @param suggested_filename possible name representing the URL
 * @param content_id_url if not NULL, receives a cid: URL of content_id
 * @param context_id if not 0, URL_Rep_Context will be created instead of URL_Rep.
 *     Enable the tweak TWEAK_MIME_SEPARATE_URL_SPACE_FOR_MIME to use this parameter.
 * @return URL object represinting the attachment
 */
URL	ConstructFullAttachmentURL_L(URLType base_url_type, BOOL no_store, HeaderEntry *content_id, const OpStringC &ext0, 
								 HeaderEntry *content_type, const OpStringC &suggested_filename, URL *content_id_url
#ifdef URL_SEPARATE_MIME_URL_NAMESPACE
								, URL_CONTEXT_ID context_id=0
#endif
							);

#endif // _MIME_SUPPORT_

/**
 * Get a line separated by CRLF, CR or LF from a text buffer
 * @param data input buffer, doesn't need to end by 0
 * @param startpos starting offset in data
 * @param data_len length of data
 * @param nextline_pos offset of the next line after the current line
 * @param length length of the current line from startpos
 * @param no_more TRUE if data_len is the final length of data
 * @return TRUE if end of line was found in the current range <data + startpos, data + data_len)
 */
BOOL GetTextLine(const unsigned char *data, unsigned long startpos, unsigned long data_len, 
				 unsigned long &nextline_pos, unsigned long &length, BOOL no_more);

/**
 * Construct a URL object from a cid: address
 * @param content_id Content-ID identifier
 * @param context_id if not 0, URL_Rep_Context will be created instead of URL_Rep.
 *     Enable the tweak TWEAK_MIME_SEPARATE_URL_SPACE_FOR_MIME to use this parameter.
 * @return URL object of the content_id or empty URL for invalid content_id
 */
URL	ConstructContentIDURL_L(const OpStringC8 &content_id
#ifdef URL_SEPARATE_MIME_URL_NAMESPACE
							, URL_CONTEXT_ID context_id=0
#endif
							);

void InheritExpirationDataL(URL_InUse &child, URL_Rep *parent);

#endif
