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

#include "core/pch.h"


#ifdef _MIME_SUPPORT_
#include "modules/mime/mimedec2.h"
#include "modules/mime/mimeutil.h"

#include "modules/olddebug/tstdump.h"


MIME_Multipart_Related_Decoder::MIME_Multipart_Related_Decoder(HeaderList &hdrs, URLType url_type)
: MIME_Multipart_Decoder(hdrs, url_type)
{
}

MIME_Multipart_Related_Decoder::~MIME_Multipart_Related_Decoder()
{
}

void MIME_Multipart_Related_Decoder::HandleHeaderLoadedL()
{
	MIME_Multipart_Decoder::HandleHeaderLoadedL();

	if(content_type_header)
	{
		ParameterList *parameters = content_type_header->SubParameters();

		Parameters *param = parameters->GetParameterByID(HTTP_General_Tag_Start, PARAMETER_ASSIGNED_ONLY);

		if(param && param->Value())
		{
			URL temp_url = ConstructContentIDURL_L(param->Value()
#ifdef URL_SEPARATE_MIME_URL_NAMESPACE
					, GetContextID()
#endif
					);
			related_start_url.SetURL(temp_url);
		}
	}

	if(GetContentLocationBaseURL().IsEmpty() && cloc_url.IsEmpty())
	{

		cloc_url = ConstructAttachmentURL_L(NULL, NULL, NULL,NULL);
		SetContentLocationBaseURL(cloc_url);
		info.cloc_base_is_local = TRUE;
	}
}

MIME_Decoder *MIME_Multipart_Related_Decoder::GetRootPart()
{
	MIME_Decoder *root = sub_elements.First();
	if (!related_start_url->IsEmpty())
	{
		while (root)
		{
			if (root->GetAttachmentURL() == related_start_url.GetURL() ||
				root->GetContentIdURL()  == related_start_url.GetURL())
				break;
			root = root->Suc();
		}
		if (!root)
			root = sub_elements.First();
	}
	return root;
}

URL MIME_Multipart_Related_Decoder::GetRelatedRootPart()
{
	MIME_Decoder *root = GetRootPart();
	return root ? root->GetAttachmentURL() : URL();
}

void MIME_Multipart_Related_Decoder::WriteDisplayDocumentL(URL &target, DecodedMIME_Storage *attach_target)
{
	if(!info.finished)
		return; // wait until finished

	current_display_item = GetRootPart();
	if(current_display_item)
		current_display_item->RetrieveDataL(target, attach_target);

	info.displayed = TRUE;
}

void MIME_Multipart_Related_Decoder::WriteDisplayAttachmentsL(URL &target, DecodedMIME_Storage *attach_target, BOOL display_as_links)
{
	display_attachment_list = TRUE;
	if (current_display_item)
	{
		OpAutoVector<SubElementId> subElementIds;
		BOOL allDisplayableInline = TRUE;
		for (MIME_Decoder* mdec = sub_elements.First(); mdec; mdec = mdec->Suc())
			if (mdec != current_display_item)
			{
				if (mdec->IsDisplayableInline())
				{
					SubElementId* subElementId = OP_NEW(SubElementId, (mdec, mdec->headers));
					if (subElementId && !OpStatus::IsSuccess(subElementIds.Add(subElementId)))
						OP_DELETE(subElementId);
				}
				else
					allDisplayableInline = FALSE;
			}
		if (current_display_item->ReferenceAll(subElementIds) && allDisplayableInline)
			display_attachment_list = FALSE;
	}

	MIME_Multipart_Decoder::WriteDisplayAttachmentsL(target, attach_target, display_as_links);
}


SubElementId::SubElementId(MIME_Decoder* decoder, HeaderList& headers)
{
	mdec = decoder;
	HTTP_Header_Tags headerTags[2] = {HTTP_Header_Content_ID, HTTP_Header_Content_Location};
	for (int i=0; i<2; i++)
	{
		id[i] = NULL;
		idLength[i] = 0;
		HeaderEntry* entry = headers.GetHeaderByID(headerTags[i]);
		if (entry && entry->Value()) {
			const char* str = entry->Value();
			unsigned long strLen = (unsigned long)op_strlen(str);
			const char* pos;
			// Remove enclosing '<' '>'
			if (str[0] == '<' && str[strLen-1] == '>')
				str++, strLen-=2;
			// Remove everything after the first '&' ('&' in urls may be escaped, making the search miss)
			if ((pos = op_strchr(str,'&')) != NULL)
				strLen = (unsigned long)(pos-str);
			// Remove everything up to the last '/' that is not at the end
			for (pos=str+strLen-2; pos>=str; pos--)
				if (*pos == '/') {
					strLen -= (unsigned long)(pos+1-str);
					str = pos+1;
					break;
				}
			// Remove trailing '/'
			if (strLen>0 && str[strLen-1] == '/')
				strLen--;
			// If the id is too short, the chance of false positive detection
			// is too high. It is then better to list the attachment
			if (strLen < 4)
				strLen = 0;
			id[i] = strLen ? str : NULL;
			idLength[i] = strLen;
		}
	}
	found = FALSE;
}

unsigned long SubElementId::MaxLength(unsigned long maxLength)
{
	for (int i=0; i<2; i++)
		if (maxLength < idLength[i])
			maxLength = idLength[i];
	return maxLength;
}

BOOL SubElementId::FindIn(const uni_char* buffer, unsigned long length)
{
	for (int i=0; i<2 && !found; i++) {
		const unsigned char* str = (const unsigned char*)id[i];
		unsigned long strLen = idLength[i];
		if (str && length >= strLen) {
			uni_char first = (uni_char)str[0];
			for (unsigned long pos=0; pos<length-strLen+1; pos++)
				if (buffer[pos] == first) {
					unsigned long off;
					for (off=1; off<strLen; off++)
						if (buffer[pos+off] != (uni_char)str[off])
							break;
					if (off == strLen) {
						found = TRUE;
						break;
					}
				}
		}
	}
	if (found)
		mdec->SetDisplayed(TRUE);

	return found;
}


#endif // MIME_SUPPORT
