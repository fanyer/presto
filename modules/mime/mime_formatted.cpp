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

#include "modules/hardcore/mh/messages.h"

#include "modules/url/url_rep.h"
#include "modules/url/url_ds.h"
#include "modules/mime/mime_cs.h"

#ifdef URL_SEPARATE_MIME_URL_NAMESPACE
#include "modules/url/url_man.h"
#endif

#include "modules/olddebug/tstdump.h"


MIME_Formatted_Payload::MIME_Formatted_Payload(HeaderList &hdrs, URLType url_type)
: MIME_Payload(hdrs, url_type)
{
}

void MIME_Formatted_Payload::WriteDisplayDocumentL(URL &target, DecodedMIME_Storage *attach_target)
{
	if(!alias.IsEmpty())
		attach_target->AddMIMEAttachment(alias, FALSE);
	if(!alias1.IsEmpty())
		attach_target->AddMIMEAttachment(alias1, FALSE);
	if(!content_id_url.IsEmpty())
		attach_target->AddMIMEAttachment(content_id_url, FALSE);
	if(!attachment->IsEmpty())
		attach_target->AddMIMEAttachment(attachment, info.displayed);

	if(attachment->IsEmpty() || !info.finished || is_attachment)
		return;

	/*
	if(!alias.IsEmpty())
	{
		target.SetAttributeL(URL::KHTTPContentLocation, alias.GetAttribute(URL::KName_Username_Password_Escaped_NOT_FOR_UI)); // Password needed, can be used by links to this object
		URL_DataStorage *url_ds = target.GetRep()->GetDataStorage();
		if(url_ds)
		{
			url_ds->BroadcastMessage(MSG_MULTIPART_RELOAD, target.Id(), 0, MH_LIST_ALL);
			url_ds->BroadcastMessage(MSG_INLINE_REPLACE, target.Id(), 0, MH_LIST_ALL);
			url_ds->BroadcastMessage(MSG_HEADER_LOADED, target.Id(), (target.GetRep()->GetIsFollowed() ? 0 : 1), MH_LIST_ALL);
		}
	}
	*/
	if(!cloc_url.IsEmpty())
		attachment->SetAttributeL(URL::KBaseAliasURL, cloc_url);
	else if(!cloc_base_url.IsEmpty())
		attachment->SetAttributeL(URL::KBaseAliasURL, cloc_base_url);

	attach_target->AddMIMEAttachment(attachment, FALSE, TRUE, TRUE);

	ANCHORD(OpString, tmp_url);
	tmp_url.Append(UNI_L("<div class=\"htmlpart\"> <object "));
	if (script_embed_policy == MIME_EMail_ScriptEmbed_Restrictions)
		tmp_url.Append(UNI_L("class=\"bodypart_mail\" "));
	tmp_url.AppendFormat(UNI_L("id=\"omf_body_part_%d\" type=\"text/html\" data=\""), attach_target->GetBodyPartCount());
	target.WriteDocumentData(URL::KNormal, tmp_url);
	attachment->GetAttributeL(URL::KUniName_Escaped, 0, tmp_url); // MIME URLs does not have passwords
	target.WriteDocumentData(URL::KXMLify, tmp_url);
	target.WriteDocumentData(URL::KNormal, UNI_L("\">Mail body</object></div>\r\n"));
	
#if defined(NEED_URL_MIME_DECODE_LISTENERS)
	// Multipart parts that have filename or name should get save buttons
	if (InList() && (filename.HasContent() || name.HasContent()) && target.GetAttribute(URL::KMIME_HasAttachmentListener))
		target.SetAttributeL(URL::KMIME_SignalAttachmentListeners, attachment);
#endif		// M2 

	info.displayed = TRUE;
#ifdef URL_SEPARATE_MIME_URL_NAMESPACE
	URL_CONTEXT_ID ctx_id = target.GetRep()->GetContextId();
	if(ctx_id != GetContextID())
	{
		target.SetAttributeL(URL::KBaseAliasURL, attachment);
	}
#endif
}

BOOL MIME_Formatted_Payload::ReferenceAll(OpVector<SubElementId>& ids)
{
	if (attachment->IsEmpty())
		return FALSE;

	UINT32 i;
	unsigned long maxStringLength = 0;
	for (i=0; i<ids.GetCount(); i++)
		maxStringLength = ids.Get(i)->MaxLength(maxStringLength);
	if (maxStringLength == 0)
		return FALSE; // Precaution

	// Create descriptor
	ParameterList* params = content_type_header ? content_type_header->SubParameters() : NULL;
	Parameters *param = params ? params->GetParameterByID(HTTP_General_Tag_Charset) : NULL;
	if (param)
		attachment->SetAttributeL(URL::KMIME_CharSet, param->Value());
	MessageHandler* mh = base_url ? base_url->GetFirstMessageHandler() : NULL;
	Window* window = mh ? mh->GetWindow() : NULL;
	URL_DataDescriptor *desc = attachment->GetDescriptor(NULL,URL::KNoRedirect,FALSE,TRUE,window);
	if (!desc)
		return FALSE;

	BOOL more;
	UINT32 numFound = 0;
	for (i=0; i<ids.GetCount(); i++)
		if (ids.Get(i)->Found())
			numFound++;
	BOOL allFound = numFound == ids.GetCount();
	do
	{
		unsigned long len = UNICODE_DOWNSIZE(desc->RetrieveData(more));
		uni_char* buffer = (uni_char *) desc->GetBuffer();
		for (i=0; i<ids.GetCount(); i++)
			if (!ids.Get(i)->Found() && ids.Get(i)->FindIn(buffer, len)) 
				allFound = ++numFound == ids.GetCount();
		if (len-1 < maxStringLength-1) // Must consume more than one character each time.
			break;
		desc->ConsumeData(UNICODE_SIZE(len-1-maxStringLength+1));
	}
	while (more && !allFound);

	OP_DELETE(desc);
	
	return allFound;
}

#endif // MIME_SUPPORT
