/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve Pettersen
 */
#include "core/pch.h"

#ifdef _MIME_SUPPORT_
#include "modules/mime/mimedec2.h"

#include "modules/util/htmlify.h"
#ifdef SMILEY_SUPPORT
# include "adjunct/desktop_util/string/htmlify_more.h"
#endif

#include "modules/mime/mime_cs.h"
#include "modules/url/url_rep.h"
#ifdef SUPPORT_TEXT_DIRECTION
#include "modules/unicode/unicode.h"
#endif

#include "modules/olddebug/tstdump.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"


MIME_Text_Payload::MIME_Text_Payload(HeaderList &hdrs, URLType url_type)
: MIME_Payload(hdrs, url_type)
{
	m_plaintext_current_quotelevel = 0;
    m_plaintext_previous_line_flowed = FALSE;
    m_plaintext_first_line_on_level = TRUE;
    m_plaintext_has_signature = FALSE;
	text_desc = NULL;
}

MIME_Text_Payload::~MIME_Text_Payload()
{
	OP_DELETE(text_desc);
	text_desc = NULL;
}

BOOL MIME_Text_Payload::PeekIsTextRTL(URL_DataDescriptor *text_desc)
{
#ifdef SUPPORT_TEXT_DIRECTION
	BOOL more=TRUE;
	unsigned long len = UNICODE_DOWNSIZE(text_desc->RetrieveData(more));
	uni_char* buffer = (uni_char *) text_desc->GetBuffer();
	while (len--) {
		switch (Unicode::GetBidiCategory(*buffer++))
		{
			case BIDI_R:
			case BIDI_AL:
				return TRUE;
			case BIDI_L:
				return FALSE;
		}
	}
#endif
	return FALSE; // Fall-back if not enough text in first block (should be extremely rare)
}

void MIME_Text_Payload::WriteDisplayDocumentL(URL &target, DecodedMIME_Storage *attach_target)
{
	if(!alias.IsEmpty())
		attach_target->AddMIMEAttachment(alias, FALSE);
	if(!alias1.IsEmpty())
		attach_target->AddMIMEAttachment(alias1, FALSE);
	if(!content_id_url.IsEmpty())
		attach_target->AddMIMEAttachment(content_id_url, FALSE);
	if(!attachment->IsEmpty())
		attach_target->AddMIMEAttachment(attachment, info.displayed);

	//Initialize XML document
    OP_MEMORY_VAR int format_is_flowed = 0; //0:Not flowed, 1:Flowed, 2:Flowed,DelSp=Yes
	if(!text_desc)
	{
        //Check if format=flowed
	    if(content_type_header)
	    {
		    ParameterList* params = content_type_header->SubParameters();
		    if(params)
		    {
				Parameters *param = params->GetParameterByID(HTTP_General_Tag_Charset);

				if(param)
					attachment->SetAttributeL(URL::KMIME_CharSet, param->Value());

			    param = params->GetParameterByID(HTTP_General_Tag_Format,PARAMETER_ASSIGNED_ONLY);
				format_is_flowed = (param && param->GetValue().CompareI("flowed")==0) ? 1 : 0;

				if (format_is_flowed > 0)
				{
					param = params->GetParameterByID(HTTP_General_Tag_Delsp,PARAMETER_ASSIGNED_ONLY);
					format_is_flowed += (param && param->GetValue().CompareI("yes")==0) ? 1 : 0;
				}
            }
        }

        //Create descriptor
		MessageHandler* mh = base_url ? base_url->GetFirstMessageHandler() : NULL;
		Window* window = mh ? mh->GetWindow() : NULL;
		text_desc = attachment->GetDescriptor(NULL,URL::KNoRedirect,FALSE,TRUE,window);
		BOOL rtl = text_desc && PeekIsTextRTL(text_desc);

		mimepart_object_url = MIME_Decoder::ConstructAttachmentURL_L(NULL,  NULL, NULL, UNI_L("plain.xml"));
		if(mimepart_object_url.IsEmpty())
			LEAVE(OpStatus::ERR_NO_MEMORY);

		attach_target->AddMIMEAttachment(mimepart_object_url, FALSE, TRUE, TRUE);

		mimepart_object_url.SetAttributeL(URL::KMIME_ForceContentType, "text/xml; charset=utf-16");
		mimepart_object_url.SetAttribute(URL::KLoadStatus, URL_LOADING);

		{
			uni_char temp = 0xfeff; // unicode byte order mark
			mimepart_object_url.WriteDocumentData(URL::KNormal, &temp,1);
		}

		ANCHORD(OpString, tmp_url);

		tmp_url.Append(UNI_L("<div class=\"plainpart\"> <object "));
		if (script_embed_policy == MIME_EMail_ScriptEmbed_Restrictions)
			tmp_url.Append(UNI_L("class=\"bodypart_mail\" "));
		tmp_url.AppendFormat(UNI_L("id=\"omf_body_part_%d\" type=\"application/vnd.opera.omf+xml\" data=\""), attach_target->GetBodyPartCount());
		target.WriteDocumentData(URL::KNormal, tmp_url);
		mimepart_object_url.GetAttributeL(URL::KUniName_Escaped, 0, tmp_url); // MIME URLs does not have passwords
		target.WriteDocumentData(URL::KXMLify, tmp_url);
		target.WriteDocumentData(URL::KNormal, UNI_L("\">Mail body</object></div>\r\n"));

		mimepart_object_url.WriteDocumentData(URL::KNormal, UNI_L("<?xml version=\"1.0\" encoding=\"utf-16\"?>\r\n"));
		mimepart_object_url.WriteDocumentData(URL::KNormal, UNI_L("<omf:mime xmlns:omf=\"http://www.opera.com/2003/omf\" xmlns:html=\"http://www.w3.org/1999/xhtml\">\r\n"));
#ifdef _LOCALHOST_SUPPORT_
		ANCHORD(OpString, mimestyle_path);
		TRAPD(rc, g_pcfiles->GetFileURLL(PrefsCollectionFiles::StyleMIMEFile, &mimestyle_path));
		if (OpStatus::IsSuccess(rc))
		{
			mimepart_object_url.WriteDocumentData(URL::KNormal, UNI_L("<html:link rel=\"stylesheet\" href=\""));
			mimepart_object_url.WriteDocumentData(URL::KXMLify, mimestyle_path);
			mimepart_object_url.WriteDocumentData(URL::KNormal, UNI_L("\" />\r\n"));
		}
#endif
		mimepart_object_url.WriteDocumentData(URL::KNormal, UNI_L("<omf:body xml:id='omf_body_start'>\r\n"));
		mimepart_object_url.WriteDocumentData(URL::KNormal, UNI_L("<omf:div xml:id='"));
		mimepart_object_url.WriteDocumentData(URL::KNormal, (format_is_flowed>=1 ? UNI_L("flowed") : UNI_L("wrapped")));
		mimepart_object_url.WriteDocumentData(URL::KNormal, UNI_L("' html:class='plaintext"));
		mimepart_object_url.WriteDocumentData(URL::KNormal, rtl ? UNI_L(" rtl") : UNI_L(""));
		mimepart_object_url.WriteDocumentData(URL::KNormal, UNI_L("'>\r\n"));
		mimepart_object_url.WriteDocumentData(URL::KNormal, /*is_external_body ? UNI_L("<omf:ql>\r\n") : */UNI_L("<omf:ql html:class='L0'>\r\n"));
		mimepart_object_url.WriteDocumentData(URL::KNormal, UNI_L("<omf:p>"));

		mimepart_object_url.SetAttributeL(URL::KIsGeneratedByOpera, TRUE);
		m_plaintext_current_quotelevel = 0;
		m_plaintext_previous_line_flowed = FALSE;
		m_plaintext_first_line_on_level = TRUE;
		m_plaintext_has_signature = FALSE;
	}
    int old_quotelevel = m_plaintext_current_quotelevel;

#ifdef SMILEY_SUPPORT
	BOOL smilies = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseSmileyImages);
#endif


	ANCHORD(OpString, body_comment_string);
	BOOL body_comment_mode = FALSE;
	unsigned long len=0;
	uni_char *buffer;
    BOOL more=TRUE;
	while(more)
	{
		target.GetAttributeL(URL::KBodyCommentString, body_comment_string);
		if (!body_comment_string.HasContent() && !text_desc)
		{
			more = FALSE;
			len = 0;
			buffer = NULL;
		}
		else if (body_comment_string.HasContent())
		{
			OpStatus::Ignore(body_comment_string.Append(UNI_L("\r\n")));
			len = body_comment_string.Length();
			buffer = body_comment_string.CStr();
			target.SetAttributeL(URL::KBodyCommentString, NULL);
			body_comment_mode = TRUE;
			more = text_desc!=NULL;
		}
		else
		{
			len = UNICODE_DOWNSIZE(text_desc->RetrieveData(more));
			buffer = (uni_char *) text_desc->GetBuffer();
			body_comment_mode = FALSE;
		}

		if(len == 0 || buffer == NULL)
			break;

		if(more && !body_comment_mode)
		{
			BOOL colon = FALSE;
			unsigned long pos;

            uni_char temp;
			for(pos = len-1; pos > 0; pos--)
			{
				temp = buffer[pos];
				if(temp == '\n' || temp  == '\r')
				{
					pos++;
					break;
				}
				else if(temp == ':')
					colon = TRUE;
			}

			if(pos == 0 && colon)
				break;

			if(pos == 0 && len > 1024)
				pos = len;

			len=pos;
		}

        //Make buffer XML-compatible
		uni_char *converted = XHTMLify_string(buffer,len, FALSE, TRUE, format_is_flowed != 0
#ifdef SMILEY_SUPPORT
			, smilies
#endif
			);

		if (!converted)
			return;

        //Generate XML
        unsigned long pos1 = 0;
		unsigned long len1 = (unsigned long)uni_strlen(converted);
		while(pos1<len1)
		{
            //Find quotelevel
            if (/*!is_external_body && */ !m_plaintext_has_signature)
            {
                m_plaintext_current_quotelevel = 0;
                while (uni_strncmp(converted+pos1+4*m_plaintext_current_quotelevel, UNI_L("&gt;"), 4)==0) m_plaintext_current_quotelevel++;
                if (m_plaintext_current_quotelevel>31)
                    m_plaintext_current_quotelevel = 31;

                if (m_plaintext_current_quotelevel != old_quotelevel)
                {
                    mimepart_object_url.WriteDocumentData(URL::KNormal, UNI_L("</omf:p>\r\n")); //Close line from another quote level
                    m_plaintext_previous_line_flowed = FALSE;

                    //Ending quote-level(s)?
                    while (old_quotelevel > m_plaintext_current_quotelevel)
                    {
					    mimepart_object_url.WriteDocumentData(URL::KNormal, UNI_L("  </omf:ql>\r\n"));
                        old_quotelevel--;
                    }

                    //Starting quote-level(s)?
                    while (old_quotelevel < m_plaintext_current_quotelevel)
                    {
                        old_quotelevel++;
					    mimepart_object_url.WriteDocumentDataUniSprintf(UNI_L("  <omf:ql html:class='L%d'>\r\n"), old_quotelevel);
                    }

                     //Start a new line for this quote level
                    mimepart_object_url.WriteDocumentData(URL::KNormal, UNI_L("  <omf:p>"));
                    m_plaintext_first_line_on_level = TRUE;
                }
            }

            int len2 = 0, len2_without_space = 0;
			uni_char current;
			while ((pos1+len2) < len1)
			{
				current = converted[pos1+len2];
				if(current == '\r' || current == '\n')
					break;

				len2++;
                if (current!=' ')
                    len2_without_space = len2;
			}

            if (/*!is_external_body && */ !m_plaintext_has_signature && uni_strncmp(converted+pos1, UNI_L("&gt;"), 4)==0)
            {
                int quote = 4;
                while (uni_strncmp(converted+pos1+quote, UNI_L("&gt;"), 4)==0) quote+=4; //Count the trailing '>' in '>>>>> quoted text'
                len2_without_space-=quote;

                if (converted[pos1+quote]==' ') quote++; //Also count the first whitespace after trailing '>>>'s
                pos1+=quote;
                len2-=quote;
            }

            //Check for start of signature
            BOOL had_signature = m_plaintext_has_signature;
			BOOL is_signature = (m_plaintext_current_quotelevel==0 && len2==3 && (pos1+len2<len1) &&
								 converted[pos1]=='-' && converted[pos1+1]=='-' && converted[pos1+2]==' ' &&
								 (converted[pos1+3]=='\r' || converted[pos1+3]=='\n'));

            if (is_signature && !m_plaintext_has_signature)
			{
				mimepart_object_url.WriteDocumentData(URL::KNormal, UNI_L("</omf:p>\r\n  <omf:sig><omf:p>"));
                m_plaintext_has_signature = TRUE;
            }
            else if (had_signature && len2==30 && uni_strncmp(converted+pos1, UNI_L("------------------------------"), 30)==0) //Signature should not fall through to next message in digest mode (RFC1153)
            {
				mimepart_object_url.WriteDocumentData(URL::KNormal, UNI_L("</omf:p>\r\n  </omf:sig>\r\n  <omf:p>"));
                m_plaintext_has_signature = FALSE;
                m_plaintext_previous_line_flowed = FALSE;
            }

            //Write start of a new line
            if (!m_plaintext_previous_line_flowed && !m_plaintext_first_line_on_level && had_signature==m_plaintext_has_signature)
            {
				mimepart_object_url.WriteDocumentData(URL::KNormal, UNI_L("</omf:p>\r\n  <omf:p>"));
                m_plaintext_first_line_on_level = TRUE;
            }

            //Write line (or a non-breaking space if line is empty)
            if (len2_without_space>0)
            {
				if (format_is_flowed>=1 && m_plaintext_current_quotelevel==0 && converted[pos1]==' ')
				{
					pos1++; //Skip space-stuffing (RFC2646, 4.4)
					len2--;
				}

                //Start a new line
			    mimepart_object_url.WriteDocumentData(URL::KNormal, UNI_L("<omf:l>"));
                mimepart_object_url.WriteDocumentData(URL::KNormal, converted+pos1, len2 - ((format_is_flowed>=2 && len2>0 && converted[pos1+len2-1]==' ')?1:0));
                mimepart_object_url.WriteDocumentData(URL::KNormal, UNI_L("</omf:l>"));
            }
            else
            {
                mimepart_object_url.WriteDocumentData(URL::KNormal, UNI_L("<omf:l html:class='lf'>&#160;</omf:l>")); //Non-Breaking SPace
            }
			pos1 += len2;
            m_plaintext_first_line_on_level = FALSE;

            //Is current line (which will soon be previous line...) flowed? (Start of signature should _not_ flow)
            m_plaintext_previous_line_flowed = (pos1>0 && converted[pos1-1]==' ' && (!m_plaintext_has_signature || had_signature));

			//If we have more than one signature delimiter, none of them are considered flowed
			if (is_signature && m_plaintext_previous_line_flowed)
				m_plaintext_previous_line_flowed = FALSE;

            //Skip newline
            if (converted[pos1]=='\r') pos1++;
            if (converted[pos1]=='\n') pos1++;
		}

		OP_DELETEA(converted);

		if (text_desc && !body_comment_mode) //Comments don't come from text_desc, and should not be consumed
			text_desc->ConsumeData(UNICODE_SIZE(len));
	}

	if(!more && info.finished)
	{
		OP_DELETE(text_desc);
		text_desc = NULL;

#if defined(NEED_URL_MIME_DECODE_LISTENERS)
		// Multipart parts that have filename or name should get save buttons
		if (InList() && (filename.HasContent() || name.HasContent()) && target.GetAttribute(URL::KMIME_HasAttachmentListener))
			target.SetAttributeL(URL::KMIME_SignalAttachmentListeners, attachment);
#endif		// M2

		info.displayed = TRUE;
		mimepart_object_url.WriteDocumentData(URL::KNormal, UNI_L("</omf:p>\r\n"));

        while (m_plaintext_current_quotelevel >= 0)
        {
            if (m_plaintext_current_quotelevel==0 && m_plaintext_has_signature)
            {
		        mimepart_object_url.WriteDocumentData(URL::KNormal, UNI_L("  </omf:sig>\r\n"));
            }

			mimepart_object_url.WriteDocumentData(URL::KNormal, UNI_L("  </omf:ql>\r\n"));
            m_plaintext_current_quotelevel--;
        }

		mimepart_object_url.WriteDocumentData(URL::KNormal, UNI_L("</omf:div>\r\n</omf:body>\r\n</omf:mime>"));
		mimepart_object_url.WriteDocumentDataFinished();
	}
}

#endif // MIME_SUPPORT
