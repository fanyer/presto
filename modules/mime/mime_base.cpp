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
#include "modules/util/handy.h"

#include "modules/url/url_rep.h"
#include "modules/url/tools/arrays.h"
#include "modules/mime/mime_enum.h"
#include "modules/formats/uri_escape.h"

#define CONST_KEYWORD_ARRAY(name) PREFIX_CONST_ARRAY(static, KeywordIndex, name, mime)
#define CONST_KEYWORD_ENTRY(x,y) CONST_DOUBLE_ENTRY(keyword, x, index, y)
#define CONST_KEYWORD_END(name) CONST_END(name)

// Required by mul_part.cpp
CONST_KEYWORD_ARRAY(mime_encoding_keywords)
	CONST_KEYWORD_ENTRY((char *) NULL, ENCODE_UNKNOWN)
	CONST_KEYWORD_ENTRY("7bit",ENCODE_7BIT)
	CONST_KEYWORD_ENTRY("8bit",ENCODE_8BIT)
	CONST_KEYWORD_ENTRY("base64", ENCODE_BASE64)
	CONST_KEYWORD_ENTRY("binary",ENCODE_BINARY)
	CONST_KEYWORD_ENTRY("quoted-printable", ENCODE_QP)
	CONST_KEYWORD_ENTRY("x-uuencode", ENCODE_UUENCODE)
CONST_KEYWORD_END(mime_encoding_keywords)
const KeywordIndex *MIME_Encoding_Keywords(size_t &len)
{
	len = CONST_ARRAY_SIZE(mime, mime_encoding_keywords);
	return g_mime_encoding_keywords;
}

#ifdef _MIME_SUPPORT_
#include "modules/mime/mimedec2.h"

#include "modules/hardcore/mem/mem_man.h"
#include "modules/viewers/viewers.h"

#include "modules/locale/oplanguagemanager.h"
#include "modules/locale/locale-enum.h"

#include "modules/prefs/prefsmanager/collections/pc_display.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/prefs/prefsmanager/collections/pc_m2.h"


#include "modules/url/url_man.h"
#include "modules/mime/mime_cs.h"
#include "modules/mime/mimeutil.h"
#include "modules/formats/base64_decode.h"

#include "modules/olddebug/tstdump.h"

CONST_KEYWORD_ARRAY(TranslatedHeaders)
	CONST_KEYWORD_ENTRY((char*) NULL,	0)
	CONST_KEYWORD_ENTRY("Bcc",			Str::SI_MIME_TRANSLATE_BCC)
	CONST_KEYWORD_ENTRY("Cc",			Str::SI_MIME_TRANSLATE_CC)
	CONST_KEYWORD_ENTRY("Date",		Str::SI_MIME_TRANSLATE_DATE)
	CONST_KEYWORD_ENTRY("Followup-To",	Str::S_MIME_TRANSLATE_FOLLOW_UP_TO)
	CONST_KEYWORD_ENTRY("From",		Str::SI_MIME_TRANSLATE_FROM)
	CONST_KEYWORD_ENTRY("Newsgroups",  Str::S_MIME_TRANSLATE_NEWSGROUPS)
	CONST_KEYWORD_ENTRY("Organization",Str::S_MIME_TRANSLATE_ORGANIZATION)
	CONST_KEYWORD_ENTRY("Reply-To",	Str::S_MIME_TRANSLATE_REPLY_TO)
	CONST_KEYWORD_ENTRY("Resent-From",	Str::S_MIME_TRANSLATE_RESENT_FROM)
	CONST_KEYWORD_ENTRY("Subject",		Str::SI_MIME_TRANSLATE_SUBJECT)
	CONST_KEYWORD_ENTRY("To",			Str::SI_MIME_TRANSLATE_TO)
CONST_KEYWORD_END(TranslatedHeaders)

CONST_KEYWORD_ARRAY(MIME_AddressHeaderNames)
	CONST_KEYWORD_ENTRY((char *) NULL, FALSE)
	CONST_KEYWORD_ENTRY("Bcc",			TRUE)
	CONST_KEYWORD_ENTRY("Cc",			TRUE)
	CONST_KEYWORD_ENTRY("From",		TRUE)
	CONST_KEYWORD_ENTRY("To",			TRUE)
CONST_KEYWORD_END(MIME_AddressHeaderNames)


void GeneralValidateSuggestedFileNameFromMIMEL(OpString & suggested, const OpStringC &mime_type);

MIME_Decoder::MIME_Decoder(HeaderList &hdrs, URLType url_type)
: headers(hdrs)
{
	unprocessed_in.SetIsFIFO();
	unprocessed_decoded.SetIsFIFO();
	base_url_type = url_type;
	forced_charset_id = 0;

	info.decode_warning = FALSE;
	info.finished = FALSE;
	info.header_loaded = FALSE;
	info.uudecode_ended = FALSE;
	info.uudecode_started = FALSE;
	info.display_headers = FALSE;
	info.writtenheaders = FALSE;
	info.displayed = FALSE;
	info.use_no_store_flag = FALSE;
	info.cloc_base_is_local = FALSE;

	content_type_header = NULL;
	encoding = ENCODE_NONE;
	content_typeid = MIME_Other;
	script_embed_policy = MIME_EMail_ScriptEmbed_Restrictions;
#ifdef URL_SEPARATE_MIME_URL_NAMESPACE
	context_id = 0;
#endif
	parent = NULL;
	base_url = NULL;

	big_icons = FALSE;
	prefer_plain = TRUE;
	ignore_warnings = FALSE;
	nesting_depth = 0;
	total_number_of_parts = 0;
	number_of_parts_counter = &total_number_of_parts;

	headers.SetKeywordList(KeywordIndex_HTTP_MIME);
}

void MIME_Decoder::ConstructL()
{
	HandleHeaderLoadedL();
}

MIME_Decoder::~MIME_Decoder()
{
}

void MIME_Decoder::SetUseNoStoreFlag(BOOL no_store)
{
	// Will probably never want to go from TRUE to FALSE, at present
	// we keep the TRUE status on child elements in such cases
	OP_ASSERT(!(info.use_no_store_flag && !no_store));

	info.use_no_store_flag=no_store;
}

void MIME_Decoder::LoadDataL(const unsigned char *src, unsigned long src_len)
{
	if(src == NULL || src_len == 0 || info.finished)
		return;

#ifdef MIME_DEBUG
	//DumpTofile(src, src_len, "Entering LoadData", "mime.txt");
#endif

	unprocessed_in.WriteDataL(src, src_len);

#ifdef MIME_DEBUG
	//DumpTofile(unprocessed_in.GetDirectPayload(), unprocessed_in.GetLength(), "LoadData Unprocessed", "mime.txt");
#endif

	TRAPD(op_err, ProcessDataL(FALSE));
	if(OpStatus::IsError(op_err))
	{
		if( op_err == OpStatus::ERR_NO_MEMORY )
			g_memory_manager->RaiseCondition(op_err);
		RaiseDecodeWarning();
		info.finished = TRUE;
		LEAVE(op_err);
	}

#ifdef MIME_DEBUG
	PrintfTofile("mime.txt","Exiting LoadData\n");
#endif
}

/*
void MIME_Decoder::LoadDataL(const unsigned char *src)
{
	if(src && *src)
		LoadDataL(src, op_strlen((char *) src));
}
*/

/*
void MIME_Decoder::LoadDataL(FILE *src_file)
{
	if(src_file == NULL)
	{
		FinishedLoadingL();
		return;
	}

	unsigned char *src = (unsigned char *) g_memory_manager->GetTempBuf();
	unsigned long buf_size = g_memory_manager->GetTempBufLen();
	unsigned long buf_len;

	while(!feof(src_file))
	{
		buf_len = fread(src,1,buf_size,src_file);

		if(buf_len)
			LoadDataL(src, buf_len);
	}

	FinishedLoadingL();
}
*/

/*
void MIME_Decoder::LoadDataL(URL &src_url)
{
	OpStackAutoPtr<URL_DataDescriptor> desc(src_url.GetDescriptor(NULL, TRUE,TRUE));

	if(desc.get() == NULL)
	{
		FinishedLoadingL();
		return;
	}

	BOOL more = TRUE;
	while(more)
	{
		if(desc->RetrieveData(more))
		{
			LoadDataL((unsigned char *) desc->GetBuffer(), desc->GetBufSize());
			desc->ConsumeData(desc->GetBufSize());
		}
	}

	FinishedLoadingL();

	desc.reset();
}
*/

void MIME_Decoder::FinishedLoadingL()
{
	if(info.finished)
		return;

	ProcessDataL(TRUE);
	info.finished = TRUE;

	HandleDecodedDataL(FALSE);
	HandleFinishedL();
}

void MIME_Decoder::HandleFinishedL()
{
}

void MIME_Decoder::ProcessDataL(BOOL no_more)
{
	if(unprocessed_in.GetLength() == 0)
		return;

#ifdef MIME_DEBUG
	PrintfTofile("mimein.txt", "Process data %p\n",this);
	//DumpTofile(unprocessed_in.GetDirectPayload(), unprocessed_in.GetLength(), "Unprocessed data","mimein.txt");
#endif

	unsigned long pos =0;
	unsigned long end_of_line = 0;
	unsigned long unprocessed_count;

	unprocessed_count= unprocessed_in.GetLength();

	while(pos < unprocessed_count)
	{
		unsigned long startpos = pos;
		{
			// decode encoding and process
			switch(encoding)
			{
			case ENCODE_NONE:
			case ENCODE_7BIT:
			case ENCODE_8BIT:
			case ENCODE_BINARY:
				if(unprocessed_decoded.Capacity() < unprocessed_decoded.GetLength() + unprocessed_count)
					unprocessed_decoded.ResizeL(unprocessed_decoded.GetLength() + unprocessed_count, TRUE);
				unprocessed_decoded.AddContentL(&unprocessed_in, 0, unprocessed_count/20);
				HandleDecodedDataL(TRUE);
				pos = unprocessed_count=0;
				break;
			case ENCODE_QP:
				pos = DecodeQuotedPrintableL(no_more);//FIXME:OOM How should errors be handled?
				break;
			case ENCODE_BASE64:
				pos = DecodeBase64L(no_more);//FIXME:OOM How should errors be handled?
				break;
			case ENCODE_UUENCODE:
				pos = DecodeUUencodeL(no_more);//FIXME:OOM How should errors be handled?
				break;
			}

			end_of_line= startpos = pos;

			if(!no_more)
				break;
		}

		/*
		if(info.incomplete_line && pos == 0)
			break;
		*/

		if(pos > 0 && startpos > 0 && unprocessed_count > pos)
		{
			unsigned long movepos = end_of_line;

			if(startpos < movepos)
				movepos = startpos;

#ifdef MIME_DEBUG
			//DumpTofile(unprocessed_data, movepos,"Process consuming data #1","mime.txt");
#endif
			unprocessed_in.CommitSampledBytesL(movepos);

#ifdef MIME_DEBUG
			DumpTofile(unprocessed_in.GetDirectPayload(), unprocessed_in.GetLength(),"Process remaining data #1","mime.txt");
#endif
			end_of_line -= movepos;
			pos -= movepos ;
			startpos -= movepos;
		}
		unprocessed_count= unprocessed_in.GetLength();
	}

	if(pos > 0)
	{
		if(unprocessed_count > pos)
		{
#ifdef MIME_DEBUG
			//DumpTofile(unprocessed_data, end_of_line,"Process consuming data #1","mime.txt");
#endif
			unprocessed_in.CommitSampledBytesL(pos);

#ifdef MIME_DEBUG
			DumpTofile(unprocessed_in.GetDirectPayload(), unprocessed_in.GetLength(),"Process remaining data #1","mime.txt");
#endif
		}
		else
		{
#ifdef MIME_DEBUG
			//DumpTofile(unprocessed_data, unprocessed_count,"Process consuming data #1","mime.txt");
#endif
			unprocessed_in.CommitSampledBytesL(unprocessed_count);
		}
	}
}

unsigned long MIME_Decoder::DecodeQuotedPrintableL(BOOL no_more)
{
	if(unprocessed_in.GetLength() == 0)
		return 0;

	unsigned char *tempbuf = (unsigned char *) g_memory_manager->GetTempBuf2k();
	unsigned long tempbuf_len = g_memory_manager->GetTempBuf2kLen();

	unsigned long pos =0;
	unsigned long next_line;
	unsigned long line_length;
	unsigned long target_pos = 0;
	unsigned long i;
	unsigned char tempcode;
	BOOL completeline;
	unsigned long unprocessed_count;
	const unsigned char *unprocessed_data;

	unprocessed_data = unprocessed_in.GetDirectPayload();
	unprocessed_count= unprocessed_in.GetLength();

	if(unprocessed_decoded.Capacity() < unprocessed_decoded.GetLength() + unprocessed_count)
		unprocessed_decoded.ResizeL(unprocessed_decoded.GetLength() + unprocessed_count, TRUE);

	while(pos < unprocessed_count)
	{
		completeline = GetTextLine(unprocessed_data, pos, unprocessed_count,next_line,line_length, no_more);

		if((!completeline && !no_more) || target_pos + line_length > unprocessed_count)
			break;

		while(line_length>0 && op_isspace((unsigned char) unprocessed_data[pos+line_length-1]))
			line_length --;

		if(line_length >80)
			RaiseDecodeWarning();

		BOOL soft_line = FALSE;

		for(i=0;i< line_length;i++)
		{
			if(target_pos >= tempbuf_len-16)
			{
				unprocessed_decoded.WriteDataL(tempbuf, target_pos);
				target_pos = 0;
			}
			tempcode = unprocessed_data[pos+i];
			if(tempcode == '=')
			{
				if(i+2<line_length)
				{
					char temp_code1 = unprocessed_data[pos+i+1];
					char temp_code2 = unprocessed_data[pos+i+2];

					if(op_isxdigit(temp_code1) && op_isxdigit(temp_code2))
					{
						tempbuf[target_pos++] = UriUnescape::Decode(temp_code1, temp_code2);
						i+=2;
						continue;
					}
				}

				soft_line = TRUE;
				unsigned long j;
				for(j=i+1; j<line_length; j++)
					if(!op_isspace(unprocessed_data[pos+j]))
					{
						soft_line = FALSE;
						tempbuf[target_pos++] = tempcode;
						RaiseDecodeWarning();
						break;
					}

				if(soft_line)
					i = line_length;
			}
			else if(tempcode > 126 || tempcode == 13 /* CR */ ||
				(op_iscntrl(tempcode) && tempcode != '\t'))
			{
				tempbuf[target_pos++] = tempcode;
				// not really necessary to raise a decoding warning here, since decoding works fine normally
				// we will raise a decoding warning again when logging to the console works
				// RaiseDecodeWarning();
			}
			else
				tempbuf[target_pos++] = tempcode;
		}
		// MB: Got crash here,
		if(pos + line_length+2 > next_line // previously used to detect encoded spaces at the end of the line, now disabled
			&& unprocessed_data[pos+line_length] != 10 /* LF */
			&& unprocessed_data[pos+line_length] != 13 /* CR */
            && (pos+line_length < unprocessed_count)) //Last line might not have a CRLF
			RaiseDecodeWarning();

		if(!soft_line && !(no_more && (pos+line_length)==unprocessed_count))
		{
			if(target_pos >= tempbuf_len-16)
			{
				unprocessed_decoded.WriteDataL(tempbuf, target_pos);
				target_pos = 0;
			}

			tempbuf[target_pos++] = 13 /* CR */;
			tempbuf[target_pos++] = 10 /* CR */;
		}

		pos = next_line;
	}

	unprocessed_decoded.WriteDataL(tempbuf, target_pos);
	target_pos = 0;

	if(no_more)
		unprocessed_decoded.FinishedAddingL();

	HandleDecodedDataL(!no_more);

	return pos;
}

unsigned long MIME_Decoder::DecodeBase64L(BOOL no_more)
{
	if(unprocessed_in.GetLength() == 0)
		return 0;

	unsigned char *tempbuf = (unsigned char *) g_memory_manager->GetTempBuf2k();
	unsigned long tempbuf_len = g_memory_manager->GetTempBuf2kLen();

	BOOL warning = FALSE;
	unsigned long pos = 0, pos1;
	unsigned long unprocessed_count;
	const unsigned char *unprocessed_data;
	unsigned long target_pos = 0;

	unprocessed_data = unprocessed_in.GetDirectPayload();
	unprocessed_count= unprocessed_in.GetLength();

	unsigned long estimated_length;
	estimated_length = ((unprocessed_count +3) *3)/4;
	if(unprocessed_decoded.Capacity() < unprocessed_decoded.GetLength() + estimated_length)
		unprocessed_decoded.ResizeL(unprocessed_decoded.GetLength() + estimated_length, TRUE);

	do{
		pos1 = 0;
		target_pos = GeneralDecodeBase64(unprocessed_data+pos, unprocessed_count-pos, pos1, tempbuf, warning, tempbuf_len);

		pos += pos1;

		if(warning)
			RaiseDecodeWarning();

		if(target_pos == 0)
		{
			if(no_more)
			{
				RaiseDecodeWarning();
				pos = unprocessed_count;
			}
		}

		unprocessed_decoded.WriteDataL(tempbuf, target_pos);
	} while(unprocessed_count > 0 && target_pos > 0);

	if(no_more)
		unprocessed_decoded.FinishedAddingL();

	HandleDecodedDataL(!no_more);

	return pos;
}

unsigned long MIME_Decoder::DecodeUUencodeL(BOOL no_more)
{
	if(unprocessed_in.GetLength() == 0)
		return 0;

	unsigned char *tempbuf = (unsigned char *) g_memory_manager->GetTempBuf2k();
	unsigned long tempbuf_len = g_memory_manager->GetTempBuf2kLen();

	unsigned long pos =0;
	unsigned long next_line;
	unsigned long line_length;
	unsigned long target_pos = 0;
	unsigned long i,j;
	BOOL completeline;
	unsigned long unprocessed_count;
	const unsigned char *unprocessed_data;

	unprocessed_data = unprocessed_in.GetDirectPayload();
	unprocessed_count= unprocessed_in.GetLength();


	if(info.uudecode_ended)
		return unprocessed_count;

	unsigned long estimated_length;
	estimated_length = ((unprocessed_count +3) *3)/4;
	if(unprocessed_decoded.Capacity() < unprocessed_decoded.GetLength() + estimated_length)
		unprocessed_decoded.ResizeL(unprocessed_decoded.GetLength() + estimated_length, TRUE);

	unsigned long pos1;
	unsigned char temp[4];  /* ARRAY OK 2009-05-15 roarl */

	pos1=pos;
	while(pos < unprocessed_count)
	{
		completeline = GetTextLine(unprocessed_data, pos, unprocessed_count, next_line, line_length, no_more);

		if(!completeline && (!no_more || !info.uudecode_started))
			break;

		if(!info.uudecode_started)
		{
			char *tempname = (char *) g_memory_manager->GetTempBuf2k();

			unsigned char temp = unprocessed_data[pos+line_length];
			((unsigned char *)unprocessed_data)[pos+line_length] = '\0';
			if(op_sscanf((const char *) unprocessed_data+pos,"begin %*3o %255[^\r\n]",tempname) == 1 &&
				*tempname != '\0')
			{
				RegisterFilenameL(tempname);
				info.uudecode_started = TRUE;
			}
			((unsigned char *)unprocessed_data)[pos+line_length] = temp;

			pos = next_line;
			continue;
		}
		else if(line_length == 3 && op_strnicmp((char *) unprocessed_data + pos, "end", 3) == 0)
		{
			info.uudecode_ended = TRUE;
			pos = unprocessed_count;
			continue;
		}

		unsigned char code = unprocessed_data[pos];
		unsigned char code1;

		code = (code-0x20)&0x3F;
		if(code == 0)
		{
			//info.uudecode_ended = TRUE;
			//pos = unprocessed_count;
			pos = next_line;
			continue;
		}
		code1 = code;

		if(target_pos >= tempbuf_len - 64)
		{
#ifdef MIME_DEBUG
			PrintfTofile("mimeout.txt", "Decoded (UUDECODE) data %p\n",this);
			DumpTofile(tempbuf, target_pos, "processing data","mimeout.txt");
#endif
			unprocessed_decoded.WriteDataL(tempbuf, target_pos);
			target_pos = 0;
		}

		for(i=0,j=0,pos1 = 1; pos1 < line_length && j < code1; pos1 ++)
		{
			code = (unprocessed_data[pos+pos1]-0x20)&0x3F;
			temp[i++] = code;
			if(i==4)
			{
				tempbuf[target_pos++] = (temp[0]<< 2) | (temp[1]>> 4);
				j++;
				if(j<code1)
				{
					tempbuf[target_pos++] = (temp[1]<< 4) | (temp[2]>> 2);
					j++;
					if(j<code1)
					{
						tempbuf[target_pos++] = (temp[2]<< 6) | (temp[3]);
						j++;
					}
				}
				i=0;
			}
		}

		if(i > 1)
		{
			j++;
			tempbuf[target_pos++] = (temp[0]<< 2) | (temp[1]>> 4);
			if(i>2)
			{
				j++;
				tempbuf[target_pos++] = (temp[1]<< 4) | (temp[2]>> 2);
				// i < 4
			}
		}

		if(j != code1)
			RaiseDecodeWarning();

		pos = next_line;
	}

#ifdef MIME_DEBUG
	PrintfTofile("mimeout.txt", "Decoded (UUDECODE) data %p\n",this);
	DumpTofile(tempbuf, target_pos, "processing data","mimeout.txt");
#endif

	unprocessed_decoded.WriteDataL(tempbuf, target_pos);
	target_pos = 0;

	if(no_more)
		unprocessed_decoded.FinishedAddingL();

	HandleDecodedDataL(!no_more);

	return pos;
}

void MIME_Decoder::HandleDecodedDataL(BOOL more)
{
	TRAPD(op_err, PerformSpecialProcessingL(AccessDecodedData(), GetLengthDecodedData()));
	if(OpStatus::IsError(op_err))
	{
		RaiseDecodeWarning();
		LEAVE(op_err);
	}

	ProcessDecodedDataL(more);
}


void MIME_Decoder::RegisterFilenameL(OpStringC8 fname)
{
}


void MIME_Decoder::HandleHeaderLoadedL()
{
	HeaderEntry *header;


	info.header_loaded = TRUE;

	header = headers.First();

	while(header)
	{
		if(header->Value())
		{
			char *pos = (char *)op_strstr(header->Value(),"=?");

			if(pos)
			{
				char *pos1 = op_strchr(pos+2,'?');

				if(pos1 && op_isalpha((unsigned char) pos1[1]) && pos1[2] == '?' &&
					op_strstr(pos1+3,"?=") != NULL)
				{
					default_charset.Set(pos+2, (int)(pos1-pos)-2);
					break;
				}
			}
		}
		header = header->Suc();
	}

	header = content_type_header = headers.GetHeaderByID(HTTP_Header_Content_Type);
	if(header && header->Value() && *header->Value())
	{
		ParameterList *parameters = header->GetParametersL(PARAM_SEP_SEMICOLON| PARAM_ONLY_SEP | PARAM_STRIP_ARG_QUOTES | PARAM_HAS_RFC2231_VALUES, KeywordIndex_HTTP_General_Parameters);

		if(parameters && parameters->First())
		{
			Parameters *param = parameters->First();

			content_typeid = FindContentTypeId(param->Name());
		}
		else
			content_type_header = NULL;
	}
	else
	{
		content_type_header = NULL;
		content_typeid = MIME_Plain_text;
	}

	header = headers.GetHeaderByID(HTTP_Header_Content_Transfer_Encoding);
	if(header && header->Value())
	{
		encoding = (MIME_encoding) CheckKeywordsIndex(header->Value(),g_mime_encoding_keywords,
						(int)CONST_ARRAY_SIZE(mime, mime_encoding_keywords));

		if(encoding == ENCODE_UNKNOWN)
		{
			RaiseDecodeWarning();
			encoding = ENCODE_BINARY;
		}

		// RFC 2045, 6.4: If an entity is of type "multipart" the Content-Transfer-Encoding
		// is not permitted to have any value other than "7bit", "8bit" or "binary".
		if ((content_typeid == MIME_MultiPart
			 || content_typeid == MIME_Alternative
			 || content_typeid == MIME_Multipart_Related
#if defined(_SUPPORT_SMIME_) || defined(_SUPPORT_OPENPGP_)
			 || content_typeid == MIME_Multipart_Encrypted
			 || content_typeid == MIME_Multipart_Signed
#endif
			) && (encoding != ENCODE_7BIT && encoding != ENCODE_8BIT && encoding != ENCODE_BINARY))
		{
			RaiseDecodeWarning();
			encoding = ENCODE_BINARY;
		}
	}
	header = headers.GetHeaderByID(HTTP_Header_Content_Location);
	if(header && header->Value() && *header->Value()/* && cloc_base_url.IsEmpty()*/)
	{
		OpString val;

		const char *scan_pos1 = header->Value();
		RemoveHeaderEscapes(val, scan_pos1, (int)op_strlen(scan_pos1));

		if(val.HasContent())
		{
			uni_char *target_pos = val.DataPtr();
			const uni_char *scan_pos;
			uni_char temp_char;

			scan_pos = target_pos;
			while ((temp_char = *(scan_pos++)) != '\0')
			{
				// URL spaces are presumed to have been %XX escaped
				if(temp_char != '\"' && !uni_isspace((unsigned char) temp_char))
					*(target_pos++) = temp_char;
			}
			*(target_pos++) = '\0';

			if(val.HasContent() && uni_strncmp(val.CStr(), "attachment:", 11)!=0)
			{
				OpString val2;
				if(val.FindFirstOf(':') == KNotFound ||	!ResolveUrlNameL(val, val2))
					val2.Empty();

				cloc_url = urlManager->GetURL(cloc_base_url, (val2.HasContent() ? val2.CStr() : val.CStr())
#ifdef URL_SEPARATE_MIME_URL_NAMESPACE
										, FALSE , GetContextID()
#endif
									);
				if(!cloc_url.IsEmpty() && cloc_base_url.IsEmpty())
				{
					cloc_base_url = cloc_url;
					info.cloc_base_is_local = TRUE;
				}
			}
		}
	}
}


void MIME_Decoder::PerformSpecialProcessingL(const unsigned char *src, unsigned long src_len)
{
	// Default: No action
}

/*
URL	MIME_Decoder::ConstructContentIDURL_L(const OpStringC8 &content_id)
{
	char *tempurl = (char *) g_memory_manager->GetTempBuf2k();

	if(content_id.HasContent())
	{
		char *tempurl1 = (char *) tempurl;
		op_strcpy(tempurl1,"cid:");
		if(op_sscanf(content_id.CStr(), " <%1000[^>]",tempurl1+4) == 1)
			return urlManager->GetURL(tempurl);
	}

	return URL();
}
*/


URL	MIME_Decoder::ConstructAttachmentURL_L(HeaderEntry *content_id, const OpStringC &ext0, HeaderEntry *content_type, const OpStringC &suggested_filename, URL *content_id_url)
{
	URL ret;

	ret = ConstructFullAttachmentURL_L(base_url_type, GetUseNoStoreFlag(), content_id, ext0, content_type, suggested_filename, content_id_url
#ifdef URL_SEPARATE_MIME_URL_NAMESPACE
										, GetContextID()
#endif
									);

	ret.SetAttribute(URL::KSuppressScriptAndEmbeds, script_embed_policy);

	return ret;
}

void MIME_Decoder::WriteStartofDocumentL(URL &target, OpStringC *mimestyleFile)
{
	if(InList())
	{
		// We are inside another document, use the normal html payload
		return;
	}

	target.SetAttributeL(URL::KMIME_ForceContentType, "text/html; charset=utf-16");

	{
		uni_char temp = 0xfeff; // unicode byte order mark
		target.WriteDocumentData(URL::KNormal, &temp,1);
	}

	target.WriteDocumentData(URL::KNormal, UNI_L("<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.0 Strict//EN\">\r\n"));
	HeaderEntry *subject = headers.GetHeader("Subject");
	ANCHORD(OpString, subj);

	if(subject)
	{
		subject->GetValueStringL(subj, NULL);
	}

	target.WriteDocumentData(URL::KNormal, UNI_L("<html>\r\n<head>\r\n<title>"));
	if(subj.HasContent())
		target.WriteDocumentData(URL::KHTMLify, subj.CStr());
	target.WriteDocumentData(URL::KNormal, UNI_L("</title>\r\n"));

	ANCHORD(OpString, mimestyle);
	if(!mimestyleFile || mimestyleFile->IsEmpty())
	{
#ifdef _LOCALHOST_SUPPORT_
		TRAPD(rc, g_pcfiles->GetFileURLL(PrefsCollectionFiles::StyleMIMEFile, &mimestyle));
		if (OpStatus::IsError(rc))
			mimestyle.Empty();
#endif
	}
	else
	{
		mimestyle.Set(mimestyleFile->CStr() +(mimestyleFile->CStr() && *(mimestyleFile->CStr()) == '/' ? 1 : 0));
	}
	if (!mimestyle.IsEmpty())
	{
		target.WriteDocumentData(URL::KNormal, UNI_L("<link rel=\"stylesheet\" href=\""));
		target.WriteDocumentData(URL::KXMLify, mimestyle.CStr());
		target.WriteDocumentData(URL::KNormal, UNI_L("\">\r\n"));
	}

	target.WriteDocumentData(URL::KNormal, UNI_L("</head>\r\n\r\n<body>\r\n"));

	target.SetAttributeL(URL::KIsGeneratedByOpera, TRUE);
}

void MIME_Decoder::WriteHeadersL(URL &target, DecodedMIME_Storage *attach_target)
{
	HeaderEntry *hdr = headers.First();

	if(hdr)
	{
		ANCHORD(OpString, headerline);
		ANCHORD(OpString, name);
		ANCHORD(OpString, name_lower);
		ANCHORD(OpString, translated_name);

		hdr = headers.First();
		while(hdr)
		{
			const char *hdr1 = (const char *) hdr->Value();

			headerline.Empty();
			if(hdr1)
			{
				BOOL is_address = attach_target && hdr->Name() && CheckKeywordsIndex(hdr->Name(), g_MIME_AddressHeaderNames, (int)CONST_ARRAY_SIZE(mime, MIME_AddressHeaderNames));

				RemoveHeaderEscapes(headerline,hdr1,(int)op_strlen(hdr1),TRUE,is_address, g_pcdisplay->GetForceEncoding());
			}

			name.SetL(hdr->Name());
			name_lower.SetL(name);
			if(name_lower.HasContent())
				uni_strlwr(name_lower.DataPtr());
			translated_name.Empty();

			Str::LocaleString name_id = Str::LocaleString(hdr->Name() ? CheckKeywordsIndex(hdr->Name(), g_TranslatedHeaders, (int)CONST_ARRAY_SIZE(mime, TranslatedHeaders)) : Str::NOT_A_STRING);
			if(name_id)
			{
				g_languageManager->GetStringL(name_id, translated_name);
			}

			target.WriteDocumentData(URL::KNormal, UNI_L("    <omf:hdr name=\""));
			target.WriteDocumentData(URL::KXMLify, name_lower);
			target.WriteDocumentData(URL::KNormal, UNI_L("\"><omf:n>"));
			target.WriteDocumentData(URL::KXMLify, translated_name.HasContent() ? translated_name : name);
			target.WriteDocumentData(URL::KNormal, UNI_L("</omf:n><omf:v>"));
			target.WriteDocumentData(URL::KXMLify, headerline);
			target.WriteDocumentData(URL::KNormal, UNI_L("</omf:v></omf:hdr>\r\n"));

			hdr = hdr->Suc();
		}

		WriteSpecialHeaders(target);
	}
}

void MIME_Decoder::WriteSpecialHeaders(URL &target)
{
}

void MIME_Decoder::WriteAttachmentListL(URL &target, DecodedMIME_Storage *attach_target, BOOL display_as_links)
{
	URLType type = base_url ? (URLType)base_url->GetAttribute(URL::KType) : URL_EMAIL;
	if (type != URL_EMAIL && type != URL_ATTACHMENT)
		return; // We don't want attachment list and warnings on anything but email (i.e. mhtml)

	if(HaveAttachments())
	{
		WriteDisplayAttachmentsL(target, attach_target);
	}

	if(HaveDecodeWarnings())
	{
#if LANGUAGE_DATABASE_VERSION >= 842
		ANCHORD(OpString, warning_message);

		g_languageManager->GetStringL(Str::S_MIME_DECODER_WARNING, warning_message);

		target.WriteDocumentData(URL::KNormal, UNI_L("<div id='qp_error'>"));
		target.WriteDocumentData(URL::KNormal, warning_message);
		target.WriteDocumentData(URL::KNormal, UNI_L("</div>\r\n"));
#else
		target.WriteDocumentData(URL::KNormal, UNI_L("<div id='qp_error'>Warning: While decoding this file Opera encountered errors.</div>\r\n"));
#endif
	}
}

void MIME_Decoder::WriteEndOfDocument(URL &target)
{

	if(InList())
	{
		// We are inside another document, use the normal html payload
		return;
	}

	target.WriteDocumentData(URL::KNormal, UNI_L("</body>\r\n</html>\r\n"));
}

BOOL MIME_Decoder::HaveDecodeWarnings()
{
	return info.decode_warning;
}

BOOL MIME_Decoder::HaveAttachments()
{
	return FALSE;
}

void MIME_Decoder::InheritAttributes(const MIME_Decoder *parent)
{
	SetParent(parent);
	SetScriptEmbed_Restrictions(parent->GetScriptEmbed_Restrictions());
	SetUseNoStoreFlag(parent->GetUseNoStoreFlag());
	SetPreferPlaintext(parent->GetPreferPlaintext());
	SetIgnoreWarnings(parent->GetIgnoreWarnings());
	nesting_depth = parent->nesting_depth+1;
	number_of_parts_counter = parent->number_of_parts_counter;
	if(!parent->cloc_base_url.IsEmpty())
	{
		URL tmp = parent->GetContentLocationBaseURL();
		SetContentLocationBaseURL(tmp);
	}
}

// Only performed when no output is generated
void MIME_Decoder::RetrieveAttachementList(DecodedMIME_Storage *attach_target)
{
}

void MIME_Decoder::RetrieveDataL(URL &target, DecodedMIME_Storage *attach_target)
{
	if(!info.header_loaded || (info.displayed && InList()))
		return;

	if((!InList() || info.display_headers) && !info.writtenheaders)
	{
		WriteStartofDocumentL(target);

		URL header_url = ConstructAttachmentURL_L(NULL,  NULL, NULL, UNI_L("headers.xml"));
		if(header_url.IsEmpty())
			LEAVE(OpStatus::ERR_NO_MEMORY);

		attach_target->AddMIMEAttachment(header_url, FALSE, TRUE);

		header_url.SetAttributeL(URL::KMIME_ForceContentType, "text/xml; charset=utf-16");
		header_url.SetAttribute(URL::KLoadStatus, URL_LOADING);

		ANCHORD(OpString, tmp_url);
		header_url.GetAttributeL(URL::KUniName_Escaped, 0, tmp_url); // MIME URLs does not have passwords

		{
			uni_char temp = 0xfeff; // unicode byte order mark
			header_url.WriteDocumentData(URL::KNormal, &temp,1);
		}

		if(content_typeid != MIME_Multipart_Related || script_embed_policy == MIME_EMail_ScriptEmbed_Restrictions)
		{
			target.WriteDocumentData(URL::KNormal, UNI_L("<div class=\"headers\"><object type=\"application/vnd.opera.omf+xml\" data=\""));
			target.WriteDocumentData(URL::KXMLify, tmp_url);
			target.WriteDocumentData(URL::KNormal, UNI_L("\">Mail headers</object></div>\r\n"));

			header_url.WriteDocumentData(URL::KNormal, UNI_L("<?xml version=\"1.0\" encoding=\"utf-16\"?>\r\n"));
			header_url.WriteDocumentData(URL::KNormal, UNI_L("<omf:mime xmlns:omf=\"http://www.opera.com/2003/omf\" xmlns:html=\"http://www.w3.org/1999/xhtml\">\r\n"));
#ifdef _LOCALHOST_SUPPORT_
			ANCHORD(OpString, mimestyle);
			TRAPD(rc, g_pcfiles->GetFileURLL(PrefsCollectionFiles::StyleMIMEFile, &mimestyle));
			if (OpStatus::IsSuccess(rc))
			{
				header_url.WriteDocumentData(URL::KNormal, UNI_L("<html:link rel=\"stylesheet\" href=\""));
				header_url.WriteDocumentData(URL::KXMLify, mimestyle.CStr());
				header_url.WriteDocumentData(URL::KNormal, UNI_L("\" />\r\n"));
			}
#endif
			header_url.WriteDocumentData(URL::KNormal, UNI_L("<omf:headers><omf:hgrp>\r\n"));
			WriteHeadersL(header_url, NULL);
			header_url.WriteDocumentData(URL::KNormal, UNI_L("</omf:hgrp></omf:headers>\r\n</omf:mime>\r\n"));
			header_url.WriteDocumentDataFinished();
		}

		header_url.SetAttributeL(URL::KIsGeneratedByOpera, TRUE);
		if(!InList() && !cloc_base_url.IsEmpty())
			target.SetAttributeL(URL::KBaseAliasURL, cloc_base_url);

		info.writtenheaders = TRUE;
	}

	if(!info.displayed)
	{
		WriteDisplayDocumentL(target, attach_target);
	}

	if(info.finished && !InList())
	{
		WriteAttachmentListL(target, attach_target);
		WriteEndOfDocument(target);
		target.WriteDocumentDataFinished();
	}
}

#endif // MIME_SUPPORT
