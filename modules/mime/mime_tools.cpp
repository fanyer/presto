/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"

#include "modules/pi/OpSystemInfo.h"

#include "modules/url/url_man.h"

#ifdef _MIME_SUPPORT_
#include "modules/mime/mimedec2.h"

#include "modules/hardcore/mem/mem_man.h"
#include "modules/viewers/viewers.h"

#include "modules/idna/idna.h"

#include "modules/encodings/charconverter.h"
#include "modules/encodings/decoders/inputconverter.h"
#include "modules/encodings/detector/charsetdetector.h"

#include "modules/prefs/prefsmanager/collections/pc_display.h"

#include "modules/olddebug/tstdump.h"
#include "modules/formats/base64_decode.h"
#include "modules/formats/uri_escape.h"

void GeneralValidateSuggestedFileNameFromMIMEL(OpString & suggested, const OpStringC &mime_type);


OP_STATUS ConvertAndAppend(OpString &target, CharConverter *converter, const char *a_source, int a_len, BOOL a_contain_email_address = FALSE)
{
	const char * OP_MEMORY_VAR source = a_source;
	OP_MEMORY_VAR int len = a_len;
	OP_MEMORY_VAR BOOL contain_email_address = a_contain_email_address;
	uni_char *tempunidata;
	int templen, tempunidatalen, tempunilen;

	if(source == NULL || len <= 0)
		return OpStatus::OK;

	tempunidata = (uni_char *) g_memory_manager->GetTempBufUni();
	tempunidatalen = g_memory_manager->GetTempBufUniLenUniChar();

	while(len > 0)
	{
		if(contain_email_address)
		{
			OP_MEMORY_VAR char temp = source[len];
			((char *)source)[len] = '\0'; // Ugly, but needed

			BOOL is_emailaddress = TRUE;
			TRAPD(op_err, IDNA::ConvertFromIDNA_L(source, tempunidata, tempunidatalen, is_emailaddress));

			((char *)source)[len] = temp; // Ugly, but needed

			if(OpStatus::IsError(op_err))
			{
				contain_email_address = FALSE;
				continue;
			}

			tempunilen = (int)uni_strlen(tempunidata);
			templen = len;
		}
		else if(converter)
		{
			templen = 0;
			tempunilen = UNICODE_DOWNSIZE(converter->Convert((char *) source, len, (char *) tempunidata, UNICODE_SIZE(tempunidatalen), &templen));
		}
		else
		{
			templen = len;

			if(templen+1 > tempunidatalen)
				templen = tempunidatalen-1;

			tempunilen = templen;

			make_doublebyte_in_buffer(source, templen, tempunidata, tempunidatalen);
		}

		OP_ASSERT(templen>0);
		if (templen==0) //Avoid infinite loop for wrongly encoded multi-byte encodings (Fixes bug #176576)
			break;

		source += templen;
		len -= templen;
		
		RETURN_IF_ERROR(UriEscape::AppendEscaped(target, tempunidata, tempunilen, UriEscape::PrefixBackslash | UriEscape::NonSpaceCtrl));
	}
	return OpStatus::OK;
}

void RemoveHeaderEscapes(OpString &target, const char *&source, int maxlen, BOOL toplevel, BOOL structured,
						 const char *def_charset, CharConverter *converter2,
						 BOOL contain_email_address)
{
	int charsetlen, encodinglen,datalen;
	const char *src, *charset,*encoding, *data;
	const char *start;
	char *tempdata, *temppos;
	int tempdatalen, templen, unprocessed=0;
	CharConverter *converter= NULL;
	CharConverter *converter3= NULL;
	const char *unconverted = NULL;
	int unconv_len = 0;

	if(source == NULL)
		return;

	if(target.Capacity() < target.Length() + maxlen +1)
	{
		if(target.Reserve(target.Length() + maxlen +1) == NULL)
			return;
	}

	if(toplevel)
	{
		int i;
		for(i=0;i<maxlen && ((unsigned char) source[i])!=0x1b && ((unsigned char) source[i]) <0x80 ;i++)
		{
#ifdef _DEBUG
//			int j=0;
#endif
		}

		if(i < maxlen)
		{
			OpString8 cset;
			CharsetDetector test(NULL,NULL);

			test.PeekAtBuffer(source, maxlen);
			cset.Set(test.GetDetectedCharset());

			if(cset.IsEmpty())
			{
				// charset decoder haven't got a clue and def. charset is not set, use system charset
				cset.Set(def_charset ? def_charset : g_op_system_info->GetSystemEncodingL());
			}

			InputConverter *new_converter=NULL;
			OP_STATUS rc = InputConverter::CreateCharConverter(cset.CStr(), &new_converter, TRUE);
			converter3 = converter2 = new_converter;
			if (OpStatus::IsSuccess(rc))
				def_charset = converter3->GetCharacterSet();
		}
	}

	tempdata = (char *) g_memory_manager->GetTempBuf2();
	tempdatalen = g_memory_manager->GetTempBuf2Len();

	while(maxlen>0)
	{
		unprocessed = 0;
		if(toplevel && structured && *source == '<')
		{
			if(unconverted)
			{
				ConvertAndAppend(target, converter2, unconverted, unconv_len);
				unconverted = NULL;
				unconv_len = 0;
			}

			// This is supposed to be normal 7-bit email addresses
			start = source;

			while(maxlen > 0 && *source != '>')
			{
				maxlen --;
				source ++;
				//*(target++) =  *(source++);
			}

			maxlen --;
			source ++;

			if(contain_email_address)
			{
				target.Append(UNI_L("<"));
				ConvertAndAppend(target, converter2, start+1, (int)(source - start-2), contain_email_address);
				target.Append(UNI_L(">"));
			}
			else
				target.Append(start, (int)(source - start));
		}
		else if(toplevel && /*structured &&*/ *source == '"')
		{
			if(unconverted)
			{
				ConvertAndAppend(target, converter2, unconverted, unconv_len);
				unconverted = NULL;
				unconv_len = 0;
			}
			temppos = tempdata;
			templen = 0;

			maxlen --;
			templen ++;
			*(temppos++) =  *(source++);
			while(maxlen > 0 && *source != '"')
			{
				if(templen < tempdatalen-4)
				{
					*temppos='\0';
					target.Append(tempdata, templen);
					temppos = tempdata;
					templen = 0;
				}

				char temp0 = *source;

				if(temp0 == '\\')
				{
					source ++;
					maxlen --;
					if(*source != '\0')
					{
						*(temppos++) = *(source);
						templen++;
						if(*source == '\r' && *(source+1) == '\n')
						{
							*(temppos++) = '\n';
							templen++;
							source ++;
							source ++;
							maxlen -=2;
							if(*source == ' ')
							{
								source++;
								maxlen --;
							}
						}
						else
						{
							source++;
							maxlen --;
						}
					}
					continue;
				}
				maxlen --;
				templen++;
				*(temppos++) =  *(source++);
			}

			if(maxlen && *source)
			{
				maxlen --;
				templen++;
				*(temppos++) =  *(source++);
			}

			if(templen)
			{
				*temppos='\0';
				ConvertAndAppend(target, converter2, tempdata, templen);
			}
		}
		else if(structured && *source == '(')
		{
			if(unconverted)
			{
				ConvertAndAppend(target, converter2, unconverted, unconv_len);
				unconverted = NULL;
				unconv_len = 0;
			}
			int level = 1;
			const char *src1 = source+1;
			int len1 = 1;

			while(len1< maxlen && level > 0)
			{
				if(*src1 == '(')
					level++;
				else if(*src1 == ')')
					level --;

				src1++;
				len1++;
			}

			if(level > 0)
			{
				ConvertAndAppend(target, converter2, source,maxlen);
				source+=maxlen;
				maxlen = 0;
			}
			else
			{
				target.Append((source++),1);
				RemoveHeaderEscapes(target,source,len1-2,FALSE, FALSE, def_charset, converter2);
				target.Append((source++),1);
				maxlen-=len1;
			}
		}
		else if(maxlen >2 && *source == '=' && source[1] == '?')
		{
			if(unconverted)
			{
				ConvertAndAppend(target, converter2, unconverted, unconv_len);
				unconverted = NULL;
				unconv_len = 0;
			}

			unprocessed = maxlen -2;
			src = source +2;

			charset = src;
			while(unprocessed > 0 && *src && *src != '?')
			{
				src++;
				unprocessed--;
			}
			charsetlen = maxlen-unprocessed-2;

			{
				// Look for RFC 2231 language segment
				int j = charsetlen;

				while(j>0)
				{
					j--;
					if(charset[j] == '*')
					{
						charsetlen = j;
						break;
					}
				}
			}

			if(unprocessed <= 0 || !*src)
				goto fail_escape;

			encoding = ++src;
			unprocessed--;
			if(unprocessed <=  0 || *src == '=')
				goto fail_escape;
			while(unprocessed > 0 && *src && *src != '?')
			{
				src++;
				unprocessed--;
			}
			if(unprocessed <= 0)
				goto fail_escape;
			encodinglen = maxlen-unprocessed-3-charsetlen;

			if(unprocessed <= 0 || !*src)
				goto fail_escape;

			data = ++src;
			if(unprocessed > 0)
				unprocessed--;
			if(unprocessed <=  0)
				goto fail_escape;
			while(unprocessed > 0 && *src && (*src != '?' || (unprocessed > 1 && src[1] != '=')))
			{
				src++;
				unprocessed--;
			}
			datalen = maxlen-unprocessed-4 - charsetlen - encodinglen;
			if(*src)
				src++;
			if(unprocessed > 0)
			unprocessed--;

			char encodingtype = op_toupper(*encoding);
			OpString8 charset_str;

			if(OpStatus::IsError(charset_str.Set(charset, charsetlen)))
				goto fail_escape;

			InputConverter *new_converter=NULL;
			if (OpStatus::IsError(InputConverter::CreateCharConverter(charset_str.CStr(), &new_converter, TRUE)))
				if (OpStatus::IsError(InputConverter::CreateCharConverter(g_pcdisplay->GetDefaultEncoding(), &new_converter, TRUE)))
					goto fail_escape;
			converter = new_converter;

			if(*src != '=' || charsetlen == 0 || encodinglen != 1 || datalen == 0 ||
				(encodingtype != 'Q' && encodingtype != 'B'))
				goto fail_escape;

			temppos = tempdata;
			templen = 0;
			if(encodingtype == 'Q')
			{
				while(datalen >0)
				{

					if(templen > tempdatalen -4)
					{
						if(OpStatus::IsError(ConvertAndAppend(target,converter, tempdata, templen)))
							goto fail_escape;

						templen = 0;
						temppos = tempdata;
					}

					if(*data=='_')
						*(temppos++) = '\x20';
					else if(*data == '=')
					{
						if(datalen == 1)
							*(temppos++) = *data;
						else if(datalen == 2)
						{
							*(temppos++) = *(data++);
							*(temppos++) = *data;
							datalen --;
						}
						else
						{
							//if(!op_isxdigit(data[1]) || !op_isxdigit(data[2]))
							if(!op_isxdigit((unsigned char) data[1]) || !op_isxdigit((unsigned char) data[2])) // 01/04/98 YNP
							{
								*(temppos++) = *(data++);
								*(temppos++) = *(data++);
								*(temppos++) = *data;
								datalen -= 2;
							}
							else
							{
								*(temppos++) = UriUnescape::Decode(data[1], data[2]);
								data+=2;
								datalen -= 2;
							}
						}
					}
					else
						*(temppos++) = *data;

					data++;
					datalen --;
					templen = (int)(temppos - tempdata);
				}
			}
			else // if(encodingtype == 'B')
			{
				for(;datalen >0;)
				{
					BOOL warning = FALSE;
					unsigned long pos = 0;

					templen = datalen;
					templen = (datalen <= tempdatalen ? datalen : tempdatalen);

					unsigned long target_pos = GeneralDecodeBase64((const unsigned char*) data, templen, pos, (unsigned char *) tempdata, warning);

					if(OpStatus::IsError(ConvertAndAppend(target,converter, tempdata, target_pos)))
						goto fail_escape;

					templen = 0;

					datalen -= pos;
					data += pos;

					if(pos == 0)
						break;
				}
			}

			if(templen && OpStatus::IsError(ConvertAndAppend(target,converter, tempdata, templen)))
				goto fail_escape;

			OP_DELETE(converter);
			converter = NULL;

			source = ++src;
			maxlen = --unprocessed;

			while(unprocessed > 0 && op_isspace((unsigned char) *src)) // 01/04/98 YNP
			{
				src++;
				unprocessed --;
			}
			if(unprocessed >2 && *src == '=' && src[1] == '?')
			{
				source = src;
				maxlen = unprocessed;
			}
		}
		else
		{
			if(*source != '\0')
			{
				if(!unconverted)
				{
					unconverted= source;
					unconv_len = 0;
				}
				source ++;
				unconv_len ++;
			}
			//ConvertAndAppend(target, converter2, source++,1);
			maxlen--;
		}

		continue;
fail_escape:
		if(converter)
		{
			OP_DELETE(converter);
			converter = NULL;
		}
		if(unconverted)
		{
			ConvertAndAppend(target, converter2, unconverted, unconv_len);
			unconverted = NULL;
			unconv_len = 0;
		}
		ConvertAndAppend(target, converter2, source, maxlen - unprocessed);
		maxlen = unprocessed;
		source = src;
		continue;

	}
	if(unconverted)
	{
		ConvertAndAppend(target, converter2, unconverted, unconv_len);
		unconverted = NULL;
		unconv_len = 0;
	}

	OP_DELETE(converter3);
}

URL	ConstructFullAttachmentURL_L(URLType base_url_type, BOOL no_store, HeaderEntry *content_id,
							 const OpStringC &ext0, HeaderEntry *content_type, const OpStringC &suggested_filename, URL *content_id_url
#ifdef URL_SEPARATE_MIME_URL_NAMESPACE
							, URL_CONTEXT_ID context_id
#endif
							)
{
	if(content_id_url && content_id)
	{
		*content_id_url = ConstructContentIDURL_L(content_id->Value()
#ifdef URL_SEPARATE_MIME_URL_NAMESPACE
							, context_id
#endif
							);

		if(content_id_url->IsEmpty())
		{
			content_id = NULL;
			content_id_url = NULL;
		}
	}
	else
	{
		content_id_url = NULL;
		content_id = NULL;
	}

	const uni_char *urlpref;

	switch(base_url_type)
	{
	case URL_NEWSATTACHMENT:
		urlpref = UNI_L("newsatt");
		break;
	case URL_SNEWSATTACHMENT:
		urlpref = UNI_L("snewsatt");
		break;
	default:
		urlpref = UNI_L("attachment");
		break;
	}

	ANCHORD(OpString, content_type_tmp_str);
	if(content_type && content_type->Value())
	{
		ParameterList *parameters = content_type->GetParametersL(PARAM_SEP_SEMICOLON| PARAM_ONLY_SEP | PARAM_STRIP_ARG_QUOTES | PARAM_HAS_RFC2231_VALUES, KeywordIndex_HTTP_General_Parameters);

		if(parameters && parameters->First())
			content_type_tmp_str.Set(parameters->First()->Name());
	}

	const uni_char *ext = ext0.CStr();

	if(suggested_filename.IsEmpty() && ext0.IsEmpty())
	{
		if(content_type_tmp_str.HasContent() && content_type_tmp_str.CompareI(UNI_L("application/octet-stream")) != 0)
		{
			if (Viewer *v = g_viewers->FindViewerByMimeType(content_type_tmp_str))
			{
				const OpStringC extension = v->GetExtension(0);
				ext = extension.HasContent() ? extension.CStr() : NULL;
			}
			else
				ext = NULL;
		}
	}

	g_mime_attachment_counter++;
	uni_char *attachnum= (uni_char *) g_memory_manager->GetTempBuf2();
	const uni_char *tmp_str = (!ext ? UNI_L("tmp") : ext);
	uni_snprintf(attachnum, UNICODE_DOWNSIZE(g_memory_manager->GetTempBuf2Len()),
		UNI_L("attachment%ld.%s"),g_mime_attachment_counter, tmp_str);

	ANCHORD(OpString, candidate_filename);

	if(suggested_filename.HasContent())
	{
		candidate_filename.SetL(suggested_filename.CStr(), MIN(suggested_filename.Length(), 256)); // limit namelength to 256

		GeneralValidateSuggestedFileNameFromMIMEL(candidate_filename, content_type_tmp_str);

		uni_char *temp_buf = (uni_char*)g_memory_manager->GetTempBuf2();
		EscapeFileURL(temp_buf, candidate_filename.CStr(), TRUE);
		candidate_filename.Set(temp_buf);
	}

	uni_char *tempurl = (uni_char *) g_memory_manager->GetTempBufUni();
	uni_snprintf(tempurl, g_memory_manager->GetTempBufUniLenUniChar()-1, UNI_L("%s:/%u/%s"),urlpref, g_mime_attachment_counter,
		(candidate_filename.HasContent() ? candidate_filename.CStr() : attachnum));

	ANCHORD(URL, ret);
	ret = urlManager->GetURL(tempurl
#ifdef URL_SEPARATE_MIME_URL_NAMESPACE
					, context_id
#endif
					);

	if(content_id_url)
		content_id_url->SetAttributeL(URL::KAliasURL, ret);

	if(no_store)
		ret.SetAttributeL(URL::KCachePolicy_NoStore, TRUE);

	if (content_id)
		g_mime_module.SetContentID_L(ret, content_id->Value());

	return ret;
}

#endif // MIME_SUPPORT

BOOL GetTextLine(const unsigned char *data, unsigned long startpos, unsigned long data_len, unsigned long &nextline_pos, unsigned long &length, BOOL no_more)
{
	BOOL found_end_of_line = FALSE;
	unsigned long pos = startpos, line_len = 0;
	const unsigned char *current = data + startpos;

	while(pos < data_len)
	{
		unsigned char temp_code = *current++;
		pos++;
		// look for CRLF, LF and CR (LFCR is assumed to be end of line for different lines)
		if(temp_code == 13 /* CR */)
		{
			if (pos < data_len) {
				if(*current++ == 10 /* LF */)
					pos++;
				found_end_of_line = TRUE;
			} // Otherwise, we will have to wait until the next time to see if the CR was followed by LF
			break;
		}
		else if(temp_code == 10 /* LF */)
		{
			found_end_of_line = TRUE;
			break;
		}
		line_len++;
	}

    if (pos == data_len && no_more) //Are we at the end of the text?
        found_end_of_line = TRUE;

	nextline_pos = pos;
	length = line_len;

	return found_end_of_line;
}

URL	ConstructContentIDURL_L(const OpStringC8 &content_id
#ifdef URL_SEPARATE_MIME_URL_NAMESPACE
							, URL_CONTEXT_ID context_id
#endif
							)
{
	char *tempurl = (char *) g_memory_manager->GetTempBuf2k();

	if(content_id.HasContent())
	{
		char *tempurl1 = (char *) tempurl;
		op_strcpy(tempurl1,"cid:");
		if(op_sscanf(content_id.CStr(), " <%1000[^> \r\n]",tempurl1+4) == 1 ||
			op_sscanf(content_id.CStr(), " %1000[^> \r\n]",tempurl1+4) == 1 )
			return urlManager->GetURL(tempurl
#ifdef URL_SEPARATE_MIME_URL_NAMESPACE
							, context_id
#endif
							);
	}

	return URL();
}

void InheritExpirationDataL(URL_InUse &child, URL_Rep *parent)
{
	// Inherit Expires and Last-Modified from parent url
	time_t expires;		
	parent->GetAttribute(URL::KVHTTP_ExpirationDate, &expires);
	child->SetAttributeL(URL::KVHTTP_ExpirationDate, &expires);
	OpStringC8 lastmod(parent->GetAttribute(URL::KHTTP_LastModified));

	// If both Expires and Last-Modified are unset, set a dummy Last-Modified.
	// Prevents "Expired" from returning TRUE in certain situations as in CORE-25572
	// (If we set expiration date instead, we might use an an inappropriate expiry
	// pref depending on the content type. This way we keep that logic in "Expired")
	if (expires==0 && lastmod.IsEmpty())
		child->SetAttributeL(URL::KHTTP_LastModified, "Thu, 01 Jan 2009 00:00:00 GMT");
	else
		child->SetAttributeL(URL::KHTTP_LastModified, lastmod);
}
