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


#ifdef _MIME_SUPPORT_
#include "modules/mime/mimedec2.h"

#include "modules/hardcore/mem/mem_man.h"
#include "modules/viewers/viewers.h"

#include "modules/olddebug/tstdump.h"


MIME_Unprocessed_Text::MIME_Unprocessed_Text(HeaderList &hdrs, URLType url_type)
: MIME_MultipartBase(hdrs, url_type), encoding_type(MIME_plain_text)
{
	//primary_payload = NULL;
}

void MIME_Unprocessed_Text::HandleHeaderLoadedL()
{
	MIME_MultipartBase::HandleHeaderLoadedL();
	CreateTextPayloadElementL(TRUE);
}

void MIME_Unprocessed_Text::CreateTextPayloadElementL(BOOL first)
{
	{
		HeaderList new_list;

		if(content_type_header)
			content_type_header->DuplicateIntoL(&new_list);
		if (first) {
			HeaderEntry *header = headers.GetHeaderByID(HTTP_Header_Content_Location);
			if (header)
				header->DuplicateIntoL(&new_list);
			header = headers.GetHeaderByID(HTTP_Header_Content_Disposition);
			if (header)
				header->DuplicateIntoL(&new_list);
		}

		current_element = OP_NEW_L(MIME_Text_Payload, (new_list, base_url_type));
		current_element->InheritAttributes(this);
#ifdef URL_SEPARATE_MIME_URL_NAMESPACE
		current_element->SetContextID(GetContextID());
#endif // URL_SEPARATE_MIME_URL_NAMESPACE
		current_element->SetBaseURL(base_url);
		current_element->ConstructL();

		current_element->Into(&sub_elements);
		++*number_of_parts_counter;
	}
}


static BOOL looksLikeFilename(const char* s)
{
	const char* typical_filename_chars =
		"#$%+-./0123456789:="
		"@ABCDEFGHIJKLMNOPQRSTUVWXYZ\\_"
		"abcdefghijklmnopqrstuvwxyz~";
	return op_strspn(s, typical_filename_chars) == op_strlen(s);
}

#define NO 0
#define MAYBE 1
#define YES 2

static int looksLikeUUEncoded(const unsigned char* s, int len)
{
	int pos;
	BOOL suspicious = FALSE;

	if (len == 0)                              // End of input, can't really tell
		return YES;
	if (s[0] < '!' || s[0] > '`')              // Illegal start of line
		return NO;
	if (s[0] > 'M')                            // Atypical line length (too long)
		suspicious = TRUE;

	int code_len = ((((s[0]-0x20)&0x3F)+2)/3)*4;
	for (pos=1; pos<len && pos-1<code_len; pos++)
	{
		if (s[pos] < ' ' || s[pos] > '~')      // Non-printable (as us-ascii)
			return NO;
		if (s[pos] == ' ' || s[pos] > '`')     // Atypical uuencode char
			suspicious = TRUE;
	}
	if (pos == len)                            // End of input
		return suspicious ? MAYBE : YES;
	for (; pos<len; pos++)
	{
		if (s[pos] == '\r' || s[pos] == '\n')  // Found newline
			return suspicious ? MAYBE : YES;
		if (s[pos] < ' ' || s[pos] > '~')      // Non-printable (as us-ascii)
			return NO;
		if (s[pos] != ' ' && s[pos] != '\t' && s[pos] != '`') // Transport-padding or "0" 
			suspicious = TRUE;
	}
	return suspicious ? MAYBE : YES;
}


void MIME_Unprocessed_Text::ProcessDecodedDataL(BOOL more)
{
	unsigned long pos, unprocessed_count, start_pos;
	unsigned long line_length, next_line;
	const unsigned char *unprocessed_data;
	BOOL completeline;

	pos = 0;
	start_pos = 0;
	unprocessed_count = GetLengthDecodedData();
	unprocessed_data = AccessDecodedData();
	line_length = 0;
	next_line = 0;

	while(pos < unprocessed_count)
	{
		completeline = GetTextLine(unprocessed_data, pos, unprocessed_count, next_line,line_length, !more);

		if(!completeline)
		{
			if(!more)
				completeline = TRUE;
			else
				break;
		}

		switch(encoding_type)
		{
		case MIME_plain_text:
			// check for UUEncoded attachment
			if(line_length >STRINGLENGTH("begin ") && op_strncmp((const char *) unprocessed_data+pos, "begin ", STRINGLENGTH("begin ")) == 0)
			{
				char *tempname = (char *) g_memory_manager->GetTempBuf2k();
				char temp;
				int mode, num_scanned, looks_like_uuencoded;
				BOOL looks_like_filename, looks_like_mode;
				
				SaveDataInSubElementL(unprocessed_data+ start_pos, pos-start_pos);
				start_pos = pos;
				
				temp = unprocessed_data[pos+line_length];
				((char *)unprocessed_data)[pos+line_length] = '\0';
				num_scanned = op_sscanf((char *) unprocessed_data+pos,"begin %4o %255[^\r\n]",&mode,tempname);
				((char *)unprocessed_data)[pos+line_length] = temp;

				looks_like_mode = num_scanned==2 && mode>=0400 /* r-------- */ && mode<=0777 /* rwxrwxrwx */;
				looks_like_filename = num_scanned==2 && looksLikeFilename(tempname);
				// Peek ahead and see if it looks like uuencoded
				looks_like_uuencoded = looksLikeUUEncoded(unprocessed_data+next_line, unprocessed_count-next_line);
				
				// Heuristic to determine if it is likely that this is the start of a UUEncoded block
				if (num_scanned == 2 && tempname[0] != '\0' && looks_like_uuencoded != NO &&
					// 2 out of 3 of looks_like_mode, looks_like_filename and looks_like_uuencoded==YES:
					((looks_like_mode && (looks_like_filename || looks_like_uuencoded == YES)) ||
					 (looks_like_filename && looks_like_uuencoded == YES)))
				{
					FinishSubElementL();

					OpString8 mime_type;
					Viewer *v = g_viewers->FindViewerByFilename(tempname);
					mime_type.Set(v && v->GetContentTypeString8() && op_strchr(v->GetContentTypeString8(),'/') != NULL ? v->GetContentTypeString8() : "application/octet-stream");

					start_pos = pos;

					CreateNewBodyPartWithNewHeaderL(mime_type, tempname, "x-uuencode");
					encoding_type = MIME_UUencode;
				}
			}
#ifdef _SUPPORT_OPENPGP_
			else if (op_strncmp((char *) unprocessed_data+pos,"-----BEGIN PGP", STRINGLENGTH("-----BEGIN PGP")) == 0)
			{
				SaveDataInSubElementL(unprocessed_data+ start_pos, pos-start_pos);
				FinishSubElementL(); 
				start_pos = pos;
				
				CreateNewBodyPartWithNewHeaderL("application/pgp", "pgp-message.pgp", NULL);
				encoding_type = MIME_PGP_body;
			}
#endif
			break;
#ifdef _SUPPORT_OPENPGP_
		case MIME_PGP_body:
			{
				if (op_strncmp((char *) unprocessed_data+pos,"-----END PGP", STRINGLENGTH("-----END PGP")) == 0)
				{
					SaveDataInSubElementL(unprocessed_data+ start_pos, next_line -start_pos);
					FinishSubElementL();
					start_pos = next_line;
					encoding_type = MIME_plain_text;
					//current_element = primary_payload;
					CreateTextPayloadElementL();
				}
			}
#endif
			break;
		case MIME_UUencode:
			if(line_length == 3 && op_strnicmp((char *) unprocessed_data + pos, "end", 3) == 0)
			{
				SaveDataInSubElementL(unprocessed_data+ start_pos, next_line -start_pos);
				FinishSubElementL();
				start_pos = next_line;
				encoding_type = MIME_plain_text;
				//current_element = primary_payload;
				CreateTextPayloadElementL(FALSE);
			}
			break;
		}

		pos = next_line;
	}	

	if(start_pos< pos)
	{
		SaveDataInSubElementL(unprocessed_data+ start_pos, pos -start_pos);
		start_pos = pos;
	}

	CommitDecodedDataL(start_pos);
}

#undef YES
#undef NO
#undef MAYBE

#endif // MIME_SUPPORT
