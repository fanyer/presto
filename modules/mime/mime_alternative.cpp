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


MIME_Multipart_Alternative_Decoder::MIME_Multipart_Alternative_Decoder(HeaderList &hdrs, URLType url_type)
: MIME_Multipart_Decoder(hdrs, url_type)
{
}

MIME_Decoder *MIME_Multipart_Alternative_Decoder::FindDisplayItem()
{
	MIME_Decoder *preferred_item = NULL;
	int preferred_item_quality = 0;

	for (MIME_Decoder *item = sub_elements.Last(); item; item = item->Pred())
	{
		MIME_ContentTypeID type = item->GetContentTypeID();
		int quality = 0;

		switch(type)
		{
		case MIME_Plain_text:
			quality = prefer_plain ? 4 : 2;
			break;
		case MIME_HTML_text:
			quality = prefer_plain ? 3 : 4;
			break;
		case MIME_XML_text:
			quality = prefer_plain ? 2 : 3;
			break;
		case MIME_GIF_image:
		case MIME_JPEG_image:
		case MIME_PNG_image:
		case MIME_BMP_image:
		case MIME_SVG_image:
		case MIME_Message:
		case MIME_Mime:
			quality = prefer_plain ? 4 : 4;
			break;
		case MIME_MultiPart:
		case MIME_Alternative:
		case MIME_Multipart_Related:
			quality = prefer_plain ? 1 : 4;
			break;
		}

		if (quality > preferred_item_quality)
		{
			preferred_item_quality = quality;
			preferred_item = item;
		}
	}

	return preferred_item;
}

void MIME_Multipart_Alternative_Decoder::WriteDisplayDocumentL(URL &target, DecodedMIME_Storage *attach_target)
{
	if(!info.finished)
		return;

	MIME_Decoder *item = FindDisplayItem();

	if (item)
	{
		item->UnSetAttachmentFlag();
		item->RetrieveDataL(target, attach_target);
	}
}

void MIME_Multipart_Alternative_Decoder::WriteDisplayAttachmentsL(URL &target, DecodedMIME_Storage *attach_target, BOOL display_as_links)
{
	MIME_Decoder *display_item = FindDisplayItem();

	for (MIME_Decoder *item = sub_elements.First(); item; item = item->Suc())
		if (item == display_item ||
			item->GetContentTypeID() == MIME_calendar_text || // This part won't be preferred, but display attachment anyway
			item->GetContentTypeID() == MIME_MultiPart) // Also show the attachments of multipart/mixed parts 
		{
			if (item != display_item && item->GetSubElements() != NULL)
			{
				// Of the sub-elements of multipart/mixed parts, don't show the "main text"
				// (Perhaps it would be better to allow each MIME_Decoder to decide this locally in a virtual function.
				//  Currently I prefer handling this multipart/alternative spec violation workaround locally)
				for (MIME_Decoder* sub = item->GetSubElements()->First(); sub; sub = sub->Suc())
					switch (sub->GetContentTypeID()) {
						case MIME_Plain_text:
						case MIME_Text:
						case MIME_HTML_text:
						case MIME_XML_text:
							if (!sub->IsAttachment())
								sub->SetDisplayed(TRUE); // Pretend it has already been displayed
					}
			}
			item->SetBigAttachmentIcons(big_icons);
			item->RetrieveAttachementL(target, attach_target, TRUE/* ## */);
			// ## Explicitly true in order to mimic MIME_Multipart_Decoder::WriteDisplayAttachmentsL
		}
}

BOOL MIME_Multipart_Alternative_Decoder::ReferenceAll(OpVector<SubElementId>& ids)
{
	MIME_Decoder* item = FindDisplayItem();
	return item && item->ReferenceAll(ids);
}

#endif // MIME_SUPPORT
