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
#include "modules/mime/mime_cs.h"

#ifdef URL_SEPARATE_MIME_URL_NAMESPACE
#include "modules/url/url_rep.h"
#include "modules/url/url_man.h"
#endif

#include "modules/olddebug/tstdump.h"


MIME_Displayable_Payload::MIME_Displayable_Payload(HeaderList &hdrs, URLType url_type)
: MIME_Payload(hdrs, url_type)
{
}

void MIME_Displayable_Payload::WriteDisplayDocumentL(URL &target, DecodedMIME_Storage *attach_target)
{
	if(!alias.IsEmpty())
		attach_target->AddMIMEAttachment(alias, FALSE);
	if(!alias1.IsEmpty())
		attach_target->AddMIMEAttachment(alias1, FALSE);
	if(!content_id_url.IsEmpty())
		attach_target->AddMIMEAttachment(content_id_url, FALSE);
	if(!attachment->IsEmpty())
		attach_target->AddMIMEAttachment(attachment, info.displayed);

	if(attachment->IsEmpty())
		return;

	ANCHORD(OpString, url);
	ANCHORD(OpString, type);

	if(!cloc_url.IsEmpty())
		attachment->SetAttributeL(URL::KBaseAliasURL, cloc_url);
	else if(!cloc_base_url.IsEmpty())
		attachment->SetAttributeL(URL::KBaseAliasURL, cloc_base_url);
	attachment->GetAttributeL(URL::KUniName_Escaped, 0, url);
	type.SetL(attachment->GetAttribute(URL::KMIME_Type)); 
	if(type.IsEmpty()) 
		type.SetL("application/octet-stream");

	if(url.HasContent())
	{
		if(content_typeid == MIME_GIF_image || content_typeid == MIME_JPEG_image|| 
			content_typeid == MIME_PNG_image || content_typeid == MIME_BMP_image || 
			content_typeid == MIME_SVG_image || content_typeid == MIME_Flash_plugin)
		{
			if (!info.finished) //Wait until everything is ready, so listener isn't notified too early
				return;

			target.WriteDocumentData(URL::KNormal, UNI_L("<div class=\"attachments\"><object type=\""));
			target.WriteDocumentData(URL::KXMLify, type);
			target.WriteDocumentData(URL::KNormal, UNI_L("\" data=\""));
			target.WriteDocumentData(URL::KXMLify, url);
			target.WriteDocumentData(URL::KNormal, UNI_L("\">Attachment</object></div>\r\n"));

#if defined(NEED_URL_MIME_DECODE_LISTENERS)
			if(target.GetAttribute(URL::KMIME_HasAttachmentListener))
			{
				target.SetAttributeL(URL::KMIME_SignalAttachmentListeners, attachment);
			}
#endif		// M2 
		}
		else
		{
			if(is_attachment)
				return;

			target.WriteDocumentData(URL::KNormal, 
				UNI_L("    <div class='document'>\r\n      <object frameborder=\"0\" style=\"min-width:100%; height:auto;\" type=\""));
			target.WriteDocumentData(URL::KHTMLify, type);
			target.WriteDocumentData(URL::KNormal, UNI_L("\" data=\""));
			target.WriteDocumentData(URL::KHTMLify, url);
			target.WriteDocumentData(URL::KNormal, UNI_L("\">\r\n        "));
			target.WriteDocumentData(URL::KHTMLify_CreateLinks, url);
			target.WriteDocumentData(URL::KNormal, UNI_L("\r\n      </object>\r\n    <div>\r\n"));
		}
#ifdef URL_SEPARATE_MIME_URL_NAMESPACE
		URL_CONTEXT_ID ctx_id = target.GetRep()->GetContextId();
		if(ctx_id != GetContextID())
		{
			target.SetAttributeL(URL::KBaseAliasURL, attachment);
		}
#endif
	}

	info.displayed = TRUE;
}

#endif // MIME_SUPPORT
