/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve Pettersen
 */
#include "core/pch.h"


#ifdef _MIME_SUPPORT_
#include "modules/mime/mimedec2.h"

#include "modules/hardcore/mh/messages.h"

#include "modules/viewers/viewers.h"

#include "modules/prefs/prefsmanager/collections/pc_display.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/prefs/prefsmanager/collections/pc_m2.h"

#include "modules/url/url_man.h"
#include "modules/url/url_rep.h"
#include "modules/url/url_ds.h"
#include "modules/mime/mime_cs.h"

#include "modules/encodings/utility/charsetnames.h"

#include "modules/olddebug/tstdump.h"

#ifdef MHTML_ARCHIVE_REDIRECT_SUPPORT
#include "modules/url/url_loading.h"
#endif

void CleanFileName(OpString &name)
{
	int pos;

	while((pos = name.FindFirstOf(OpStringC16(UNI_L("/\\:")))) != KNotFound)
		name.Delete(0, pos+1);

	//remove trailing dots from suggested filename for windows.
	if(name.Length() && name[name.Length()-1]=='.')
		for (;name.Length() && name[name.Length()-1]=='.';)
			name[name.Length()-1]=0;
}

MIME_Payload::MIME_Payload(HeaderList &hdrs, URLType url_type)
: MIME_Decoder(hdrs, url_type)
{
	is_attachment = FALSE;
}

MIME_Payload::~MIME_Payload()
{
}

void MIME_Payload::HandleHeaderLoadedL()
{
	MIME_Decoder::HandleHeaderLoadedL();

	HeaderEntry *header = headers.GetHeaderByID(HTTP_Header_Content_Disposition);
	if(header)
	{
		ParameterList *parameters = header->GetParametersL((PARAM_SEP_SEMICOLON| PARAM_ONLY_SEP | PARAM_STRIP_ARG_QUOTES_GENTLY | PARAM_HAS_RFC2231_VALUES), KeywordIndex_HTTP_General_Parameters);
		
		if(parameters && !parameters->Empty())
		{
			if(parameters->First()->GetNameID() == HTTP_General_Tag_Attachment /*||
				parameters->First()->GetNameID() == HTTP_General_Tag_Inline*/)
				is_attachment = TRUE;
			
			if(name.IsEmpty())
			{
				parameters->GetValueStringFromParameterL(name, default_charset, HTTP_General_Tag_Name,PARAMETER_ASSIGNED_ONLY);
				CleanFileName(name);
			}
			if(filename.IsEmpty())
			{
				parameters->GetValueStringFromParameterL(filename, default_charset, HTTP_General_Tag_Filename,PARAMETER_ASSIGNED_ONLY);
				CleanFileName(filename);
			}
		}
	}
	if(name.IsEmpty())
	{
		header = headers.GetHeaderByID(HTTP_Header_Content_Type);

		if (header != NULL)
		{
			ParameterList *parameters = header->SubParameters();
			
			if(!parameters) 
				parameters = header->GetParametersL((PARAM_SEP_SEMICOLON| PARAM_ONLY_SEP | PARAM_STRIP_ARG_QUOTES_GENTLY | PARAM_HAS_RFC2231_VALUES), KeywordIndex_HTTP_General_Parameters);
			
			if(parameters && !parameters->Empty())
			{
				parameters->GetValueStringFromParameterL(name, default_charset, HTTP_General_Tag_Name,PARAMETER_ASSIGNED_ONLY);
				CleanFileName(name);
			}
		}
	}
}

void MIME_Payload::RegisterFilenameL(OpStringC8 fname)
{
	filename.SetL(fname);
}

URL	MIME_Payload::ConstructAttachmentURL_L(HeaderEntry *content_id, const uni_char *ext, HeaderEntry *content_type)
{
	return MIME_Decoder::ConstructAttachmentURL_L(content_id, ext, content_type, (filename.HasContent() ? filename : name), &content_id_url);
}

void MIME_Payload::ProcessDecodedDataL(BOOL more)
{
	const char *force_charset = NULL;

	if(attachment->IsEmpty())
	{
		ANCHORD(URL, temp_url);

		temp_url= ConstructAttachmentURL_L(headers.GetHeaderByID(HTTP_Header_Content_ID), NULL, content_type_header);
#ifdef MHTML_ARCHIVE_REDIRECT_SUPPORT
		if (IsValidMHTMLArchive() && !cloc_url.IsEmpty())
		{
			temp_url = cloc_url;
			if(!content_id_url.IsEmpty())
				content_id_url.SetAttributeL(URL::KAliasURL, cloc_url);
		}
#endif
		attachment.SetURL(temp_url);

		if (forced_charset_id != 0)
			force_charset = g_charsetManager->GetCharsetFromID(forced_charset_id);

		OP_STATUS op_err;
		if(content_type_header && content_type_header->Value())
		{
			MIME_ContentTypeID mimeid;
			const char *mimetype = content_type_header->Value();
			ParameterList *parameters;
			
			parameters = content_type_header->GetParametersL(PARAM_SEP_SEMICOLON| PARAM_ONLY_SEP | PARAM_STRIP_ARG_QUOTES | PARAM_HAS_RFC2231_VALUES, KeywordIndex_HTTP_General_Parameters);

			if(parameters && parameters->First())
			{
				Parameters *param = parameters->First();

				mimeid = FindContentTypeId(param->Name());
				if ((mimeid == MIME_Text || mimeid == MIME_Plain_text) && op_stricmp(param->Name(), "text/plain") != 0)
				{
					g_mime_module.SetOriginalContentTypeL(attachment, param->Name());
					mimetype = "text/plain";
				}
			}

			attachment->SetAttributeL(URL::KMIME_ForceContentType, mimetype);

			if (force_charset == NULL)
				force_charset = g_pcdisplay->GetForceEncoding();

			if (force_charset && *force_charset && !strni_eq(force_charset, "AUTODETECT-", 11))
			{
				attachment->SetAttributeL(URL::KMIME_CharSet, force_charset);
			}
		}
		else
		{
			ANCHORD(OpString8, type);

			if (force_charset == NULL)
				force_charset = (g_pcdisplay->GetForceEncoding() ? g_pcdisplay->GetForceEncoding() : "iso-8859-1");

			if (strni_eq(force_charset, "AUTODETECT-", 11))
				force_charset = "";

			LEAVE_IF_ERROR(type.SetConcat("text/plain", (*force_charset ? "; charset=" : ""), force_charset) );
			attachment->SetAttributeL(URL::KMIME_ForceContentType, type);
		}

		if(!cloc_url.IsEmpty() && cloc_url.GetAttribute(URL::KLoadStatus) == URL_UNLOADED && !(cloc_url == attachment))
		{
			cloc_url.SetAttributeL(URL::KAliasURL, attachment);
			alias = cloc_url;
		}
		else 
			alias = URL();
		
		if(!cloc_base_url.IsEmpty() && (filename.HasContent() || name.HasContent()))
		{
			URL temp_alias = urlManager->GetURL(cloc_base_url, (filename.HasContent() ? filename.CStr() : name.CStr()));
			
			if(!temp_alias.IsEmpty() && !(temp_alias == attachment) && temp_alias.GetAttribute(URL::KLoadStatus) == URL_UNLOADED)
			{
				temp_alias.SetAttributeL(URL::KAliasURL, attachment);
				alias1 = temp_alias;
			}
		}

		if(!cloc_url.IsEmpty())
			attachment->SetAttributeL(URL::KBaseAliasURL, cloc_url);
		else if(!cloc_base_url.IsEmpty())
			attachment->SetAttributeL(URL::KBaseAliasURL, cloc_base_url);

		if(content_typeid == MIME_Binary)
		{
			URL_DataStorage *url_ds = attachment->GetRep()->GetDataStorage();
			
			if(url_ds)
			{
				URLContentType ctype = URL_UNDETERMINED_CONTENT;
				ANCHORD(OpString, path);
				attachment->GetAttribute(URL::KUniPath, 0, path);

				OpStatus::Ignore(url_ds->FindContentType(ctype, NULL, NULL, path.CStr())); // Not critical if this fails

				url_ds->SetAttributeL(URL::KContentType, ctype);
				const char *mimetype = NULL;
				mimetype = g_viewers->GetContentTypeString(ctype);
				if(mimetype) 
				{
					if(op_stricmp(mimetype, "text") == 0)
						mimetype = "text/plain";

					op_err = url_ds->SetAttribute(URL::KMIME_Type,mimetype);
					if(op_err == OpStatus::ERR_NO_MEMORY)
						g_memory_manager->RaiseCondition(op_err);
					// Errors here only affect presentation of attachments, no need to abort.
					content_typeid = FindContentTypeId(mimetype);
				}
			}
		}
		
		attachment->SetAttributeL(URL::KHTTP_Response_Code, HTTP_OK); // Prevents reload of images due to Expired
		attachment->SetAttributeL(URL::KForceCacheKeepOpenFile, TRUE);
		attachment->SetAttributeL(URL::KLoadStatus, URL_LOADING);
		InheritExpirationDataL(attachment, base_url);
	}
#ifdef _DEBUG
	/*
	else
		int i = 1;
		*/
#endif

#ifdef MIME_DEBUG
	PrintfTofile("mimeatt.txt", "SaveData data %p %s\n",this, attachment->GetAttribute(URL::KName_Username_Password_Escaped_NOT_FOR_UI).CStr());
	//DumpTofile(AccessDecodedData(), GetLengthDecodedData(), "saving data","mimeatt.txt");
#endif

	attachment->WriteDocumentData(URL::KNormal, (char *) AccessDecodedData(), GetLengthDecodedData());
	CommitDecodedDataL(GetLengthDecodedData());

	if(attachment->GetRep()->GetDataStorage())
	{
		URL_DataStorage *url_ds = attachment->GetRep()->GetDataStorage();
		if(!attachment->GetAttribute(URL::KHeaderLoaded))
		{
			url_ds->BroadcastMessage(MSG_HEADER_LOADED, attachment->Id(), 0, MH_LIST_ALL);
			attachment->SetAttribute(URL::KHeaderLoaded,TRUE);
		}

		OP_ASSERT(attachment->GetAttribute(URL::KLoadStatus, URL::KFollowRedirect) == URL_LOADING || attachment->GetAttribute(URL::KLoadStatus, URL::KFollowRedirect) == URL_LOADED);
		url_ds->BroadcastMessage(MSG_URL_DATA_LOADED, attachment->Id(), 0, MH_LIST_ALL);
	}
}

#ifdef MHTML_ARCHIVE_REDIRECT_SUPPORT
BOOL MIME_Decoder::IsValidMHTMLArchiveURL(URL_Rep* url)
{
	return url && (URLContentType)url->GetAttribute(URL::KContentType) == URL_MHTML_ARCHIVE &&
		// Avoid spoofing exploits:
		url->GetAttribute(URL::KIsUserInitiated) &&	(URLType)url->GetAttribute(URL::KType) == URL_FILE;
}

BOOL MIME_Decoder::IsValidMHTMLArchive()
{
	const MIME_Decoder* top_decoder = this;
	while (top_decoder->parent)
		top_decoder = top_decoder->parent;
	return IsValidMHTMLArchiveURL(base_url) &&
		top_decoder->GetContentTypeID() == MIME_Multipart_Related &&
		!top_decoder->GetRelatedStartURL().IsEmpty() &&
		!top_decoder->cloc_url.IsEmpty();
}

#ifdef URL_SEPARATE_MIME_URL_NAMESPACE
// Cleanup item used to remove the context when the main URL in the context is deleted
class MIME_ContextRemover : public Link
{
	URL_CONTEXT_ID ctx_id;
public:
	MIME_ContextRemover(URL_CONTEXT_ID context_id) : ctx_id(context_id) {}
	~MIME_ContextRemover() { urlManager->RemoveContext(ctx_id, TRUE); }
};
#endif // URL_SEPARATE_MIME_URL_NAMESPACE
#endif // MHTML_ARCHIVE_REDIRECT_SUPPORT

void MIME_Payload::HandleFinishedL()
{
	MIME_Decoder::HandleFinishedL();
	if(!attachment->IsEmpty() && attachment->GetAttribute(URL::KLoadStatus) == URL_LOADING)
	{
		attachment->WriteDocumentDataFinished();

		OP_ASSERT(attachment->GetAttribute(URL::KLoadStatus, URL::KFollowRedirect) != URL_LOADING);

		URL_DataStorage *url_ds = attachment->GetRep()->GetDataStorage();
		if(url_ds)
		{
			if(!attachment->GetAttribute(URL::KHeaderLoaded))
			{
				url_ds->BroadcastMessage(MSG_HEADER_LOADED, attachment->Id(), 0, MH_LIST_ALL);
				attachment->SetAttribute(URL::KHeaderLoaded,TRUE);
			}

			url_ds->BroadcastMessage(MSG_URL_DATA_LOADED, attachment->Id(), 0, MH_LIST_ALL);
		}

#ifdef MHTML_ARCHIVE_REDIRECT_SUPPORT
		// If this part is the child of the top element, and the top element is a
		// multipart/related with a start-id that matches the content-id of this element,
		// and this element has a specified content-location, and we are loading a valid
		// MHTML archive, we assume the user actually wants *this* part to be the main document.
		if (parent && !parent->GetParent() && GetContentIdURL() == parent->GetRelatedStartURL() &&
			!cloc_url.IsEmpty() && IsValidMHTMLArchive() && base_url->GetDataStorage()) 
		{
#ifdef URL_SEPARATE_MIME_URL_NAMESPACE
			if (GetContextID() != 0)
			{
				urlManager->SetContextIsOffline(GetContextID(), TRUE);
				// Context should be removed when document is deleted
				MIME_ContextRemover *context_remover = OP_NEW_L(MIME_ContextRemover, (GetContextID()));
				attachment->GetRep()->AddCleanupItem(context_remover);
			}
#endif
			URL original_url(base_url, (char*)NULL);
			attachment->SetAttributeL(g_mime_module.GetOriginalURLAttribute(), original_url); // To allow display of the original url if desired
			attachment->SetAttributeL(g_mime_module.GetInternalRedirectAttribute(), TRUE); // To prevent reload during redirect
			base_url->SetAttributeL(URL::KMovedToURL, attachment);
			IAmLoadingThisURL yesIAm(attachment);
			LEAVE_IF_ERROR(base_url->GetDataStorage()->ExecuteRedirect_Stage2(FALSE));
		}
#endif // MHTML_ARCHIVE_REDIRECT_SUPPORT
	}
}

void MIME_Payload::WriteDisplayDocumentL(URL &target, DecodedMIME_Storage *attach_target)
{
	return; // Don't display anything
}

void MIME_Payload::WriteDisplayAttachmentsL(URL &target, DecodedMIME_Storage *attach_target, BOOL display_as_links)
{
	if(!alias.IsEmpty())
		attach_target->AddMIMEAttachment(alias, FALSE);
	if(!alias1.IsEmpty())
		attach_target->AddMIMEAttachment(alias1, FALSE);
	if(!content_id_url.IsEmpty())
		attach_target->AddMIMEAttachment(content_id_url, FALSE);
	if(!attachment->IsEmpty())
		attach_target->AddMIMEAttachment(attachment, info.displayed);

	if(!info.displayed && display_as_links && !attachment->IsEmpty())
	{
#if defined(NEED_URL_MIME_DECODE_LISTENERS) 
		if(target.GetAttribute(URL::KMIME_HasAttachmentListener))
		{
			target.SetAttributeL(URL::KMIME_SignalAttachmentListeners, attachment);
		}
#endif // NEED_URL_MIME_DECODE_LISTENERS

		ANCHORD(OpString, url);
		attachment->GetAttributeL(URL::KUniName_Escaped, 0, url);

		if(url.HasContent()
#if defined(NEED_URL_MIME_DECODE_LISTENERS) 
			&& g_pcm2->GetIntegerPref(PrefsCollectionM2::ShowAttachmentsInline)
#endif // NEED_URL_MIME_DECODE_LISTENERS 
			)
		{
			attachment->DumpSourceToDisk(TRUE);
			
			ANCHORD(OpString, name);
			ANCHORD(OpString, pathname);
			ANCHORD(OpString, iconurl);

			attachment->GetAttributeL(URL::KSuggestedFileName_L, name);

			target.WriteDocumentData(URL::KNormal, UNI_L("<div class=\"attachments\"><a href=\""));
			target.WriteDocumentData(URL::KXMLify, url);
			target.WriteDocumentData(URL::KNormal, UNI_L("\""));

			if(iconurl.HasContent())
			{
				target.WriteDocumentData(URL::KNormal, UNI_L("><img src=\""));
				target.WriteDocumentData(URL::KXMLify, iconurl.CStr());
				target.WriteDocumentData(URL::KNormal, UNI_L("\" alt=\"attachment\"/>"));
			}
			else 
			{
				target.WriteDocumentData(URL::KNormal, UNI_L(" class=\"unknown\">"));
			}
				
			target.WriteDocumentData(URL::KXMLify, name);
			target.WriteDocumentData(URL::KNormal, UNI_L("</a></div>\r\n"));
		}
	}	
}

BOOL MIME_Payload::HaveAttachments()
{
	if(!alias.IsEmpty())
		return TRUE;
	if(!alias1.IsEmpty())
		return TRUE;
	if(!content_id_url.IsEmpty())
		return TRUE;
	if(!info.displayed && !attachment->IsEmpty())
		return TRUE;

	return FALSE;
}

// Only performed when no output is generated
void MIME_Payload::RetrieveAttachementList(DecodedMIME_Storage *attach_target)
{
	if(!attach_target)
		return;

	if(!attachment->IsEmpty())
		attach_target->AddMIMEAttachment(attachment, TRUE);
}


void MIME_Payload::SetUseNoStoreFlag(BOOL no_store) 
{
	MIME_Decoder::SetUseNoStoreFlag(no_store);

	if(no_store && !attachment->IsEmpty())
		attachment->SetAttributeL(URL::KCachePolicy_NoStore, no_store);
}

#endif // MIME_SUPPORT
