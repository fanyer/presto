/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "adjunct/desktop_util/string/htmlify_more.h"

#include "adjunct/desktop_util/string/stringutils.h"
#include "modules/url/url_enum.h"
#include "modules/url/url_man.h"
#include "modules/util/htmlify.h"
#include "modules/formats/uri_escape.h"

#ifdef SMILEY_SUPPORT

// When adding new smileys, please make sure the first character is amongst
// the ones given in start_of_smileys below.
struct Smileys {
	const char* smiley;
	size_t	smiley_length;
	const char* smiley_xmlified;
	const char* smiley_class;
} smileys_list[] =
{{":)",		2, NULL,	"happy"},
 {":-)",	3, NULL,	"happy"},
 {"=)",		2, NULL,	"happy"},
 {":(",		2, NULL,	"unhappy"},
 {":-(",	3, NULL,	"unhappy"},
 {";)",		2, NULL,	"wink"},
 {";-)",	3, NULL,	"wink"},
 {":o",		2, NULL,	"surprised"},
 {":O",		2, NULL,	"surprised"},
 {":-O",	3, NULL,	"surprised"},
 {":D",		2, NULL,	"grin"},
 {":-D",	3, NULL,	"grin"},
 {"8-D",	3, NULL,	"cool"},
 {"8-)",	3, NULL,	"cool"},
 {":|",		2, NULL,	"indifferent"},
 {":-|",	3, NULL,	"indifferent"},
 {":'(",	3, NULL,	"cry"},
 {":@",		2, NULL,	"angry"},
 {":-@",	3, NULL,	"angry"},
 {":p",		2, NULL,	"tongue"},
 {":-p",	3, NULL,	"tongue"},
 {":P",		2, NULL,	"tongue"},
 {":-P",	3, NULL,	"tongue"},
 {"'<",		2, "'&lt;",	"pacman"},
 {NULL,		0, NULL,	NULL}};

static const uni_char* start_of_smileys = UNI_L(":;8='"); //First characters of all smileys.

INT32 GetSmiley(const uni_char* content, OpString8& smiley_tag, INT32& smiley_tag_len, const uni_char* start_of_text, unsigned int content_length, BOOL use_xml)
{
	if (!uni_strchr(start_of_smileys, *content)) //Is the start of the text one of the possible start of a smiley?
		return 0;

	if (content==start_of_text || uni_strchr(UNI_L(" \r\n\t"), *(content-1)) ) //Do we have whitespace in front of it?
	{
		struct Smileys *smiley;
		for (smiley = smileys_list; smiley->smiley_length>0; smiley++)
		{
			if (uni_strncmp(smiley->smiley, content, smiley->smiley_length) == 0) //We have found a smiley!
			{
				if (smiley->smiley_length == content_length || uni_strchr(UNI_L(" \r\n\t"), content[smiley->smiley_length])) //Do we have whitespace after it?
				{
					smiley_tag.AppendFormat(
								use_xml ?	"<html:span class=\"smiley-%s\" title=\"%s\">%s</html:span>" :
											"<span class=\"smiley-%s\" title=\"%s\">%s</span>",
								smiley->smiley_class,
								smiley->smiley_xmlified ? smiley->smiley_xmlified : smiley->smiley,
								smiley->smiley_xmlified ? smiley->smiley_xmlified : smiley->smiley);

					smiley_tag_len = smiley_tag.Length();

					return smiley->smiley_length;
				}
				break;
			}
		}
	}

	return 0;
}

#endif // SMILEY_SUPPORT.

OP_STATUS HTMLify_GrowStringIfNeeded(uni_char *&target, int& capacity, int used, int to_add)
{
	if (used+to_add >= capacity)
	{
		capacity = (int)((used+to_add)*1.5);

		uni_char* old = target;
		target = OP_NEWA(uni_char, capacity+1);
		if (!target)
		{
			OP_DELETEA(old);
			capacity = 0;
			return OpStatus::ERR_NO_MEMORY;
		}

		if (old)
		{
			uni_strncpy(target, old, used);	// NULL-termination OK
			OP_DELETEA(old);
		}
	}
	return OpStatus::OK;
}

OP_STATUS HTMLify_DoEscape(uni_char *&target, const uni_char *content, int len, BOOL no_link, BOOL use_xml, int flowed_mail, BOOL smileys) //flowed_mail: 0:Not flowed, 1:Flowed, 2:Flowed,DelSp=Yes
{
	target = NULL;
	int target_capacity = 0;
	int target_used = 0;
	RETURN_IF_ERROR(HTMLify_GrowStringIfNeeded(target, target_capacity, 0, max(10,(int)(len*1.1))));

	const char *tag;
	int taglen = 0;

	uni_char temp0;
	const uni_char *urlstart = content;
    uni_char* content_ptr = (uni_char*)content;

    int quote_depth = 0;
    while (quote_depth<len && *(content_ptr+quote_depth)=='>') quote_depth++;

	while(len > 0)
	{
//		BOOL next_first_on_line = FALSE;
		temp0 = *content_ptr;

		if(!no_link && temp0 == ':')
		{
			*((uni_char *)content_ptr) = '\0';
			int scheme_len = uni_strlen(urlstart);
			URLType url_type = urlManager->MapUrlType(urlstart);
			*((uni_char *)content_ptr) = ':';

			uni_char* start_of_url = (uni_char*)(content_ptr+1);
			if ((flowed_mail>=1 && *start_of_url==' ' && (*(start_of_url+1)=='\r' || *(start_of_url+1)=='\n')) ||
				(flowed_mail>=2 && *start_of_url==' ' && *(start_of_url+1)==' ' && (*(start_of_url+2)=='\r' || *(start_of_url+2)=='\n')))
			{
				while (*start_of_url==' ') start_of_url++;
				if (*start_of_url=='\r') start_of_url++;
				if (*start_of_url=='\n') start_of_url++;
				while (*start_of_url=='>')
				{
					start_of_url++;
					if (*start_of_url==' ') start_of_url++;
				}
			}

			if (url_type != URL_UNKNOWN && *start_of_url!=' ' && *start_of_url!='\r' && *start_of_url!='\n')
			{
                BOOL in_rfc1738_url = FALSE;
				BOOL has_left_paranthese = FALSE;

                //Do we have a <URL: http:// >-link, as defined in Appendix in RFC1738?
                uni_char* temp = (uni_char*)urlstart;
                while (temp>content && (*(temp-1)==' ' || ((flowed_mail>=1 && *(temp-1)=='\r') || *(temp-1)=='\n'))) temp--;
                if ((temp-5)>=content && uni_strnicmp(temp-5, UNI_L("<URL:"), 5)==0)
                    in_rfc1738_url = TRUE;

				target[target_used-scheme_len] = 0; //Remove already appended scheme
				target_used -= scheme_len;

				RETURN_IF_ERROR(HTMLify_GrowStringIfNeeded(target, target_capacity, target_used, (use_xml ? 14 : 9) + scheme_len));
				uni_strcpy(target+target_used, (use_xml ? UNI_L("<html:a href=\""): UNI_L("<a href=\"")));
				target_used += (use_xml ? 14 : 9);
				uni_strncpy(target+target_used, urlstart, scheme_len); // NULL-termination OK
				target_used += scheme_len;

				//while (len>0 && !isspace(*content_ptr) &&
				while (len>0 && !(((unsigned int) *content_ptr) < 256 && uni_isspace(*content_ptr)) &&
					*content_ptr != '"' && *content_ptr != '>')
				{
					if(*content_ptr == '<')
					{
						RETURN_IF_ERROR(HTMLify_GrowStringIfNeeded(target, target_capacity, target_used, 4));
						uni_strcpy(target+target_used, UNI_L("&lt;"));
						target_used += 4;
					}
					else if(*content_ptr == '&')
					{
						RETURN_IF_ERROR(HTMLify_GrowStringIfNeeded(target, target_capacity, target_used, 5));
						uni_strcpy(target+target_used, UNI_L("&amp;"));
						target_used += 5;
					}
					else if(*content_ptr == '"')
					{
						RETURN_IF_ERROR(HTMLify_GrowStringIfNeeded(target, target_capacity, target_used, 6));
						uni_strcpy(target+target_used, UNI_L("&quot;"));
						target_used += 6;
					}
					else if(((unsigned int) *content_ptr) < 256 && uni_iscntrl(*content_ptr) && (!uni_isspace(*content_ptr)  || *content_ptr == 0x0c || *content_ptr == 0x0b))
					{
						RETURN_IF_ERROR(HTMLify_GrowStringIfNeeded(target, target_capacity, target_used, 3));
						target[target_used++] = '%';
						target[target_used++] = UriEscape::EscapeFirst((char)(*content_ptr));
						target[target_used++] = UriEscape::EscapeLast((char)(*content_ptr));
					}
					else
					{
						RETURN_IF_ERROR(HTMLify_GrowStringIfNeeded(target, target_capacity, target_used, 1));
						target[target_used++] = *content_ptr;
					}
					if (*content_ptr == '(')
						has_left_paranthese = TRUE;

                    content_ptr++;
					len--;

                    if ( (in_rfc1738_url && (*content_ptr=='\r' || *content_ptr=='\n')) ||
                         (flowed_mail>=1 && *content_ptr==' ' && len>1 && (*(content_ptr+1)=='\r' || *(content_ptr+1)=='\n')) ) //Two spaces and DelSp=yes (flowed_mail==2) should not be handled here
                    {
                        const uni_char* tmp_linebreak = content_ptr;
                        int tmp_len = len;
                        int tmp_quote_depth = 0;

                        if (*content_ptr==' ' && len>0) {content_ptr++; len--;}
                        if (*content_ptr=='\r' && len>0) {content_ptr++; len--;}
                        if (*content_ptr=='\n' && len>0) {content_ptr++; len--;}
                        while (*content_ptr=='>' && len>0) {content_ptr++; len--; tmp_quote_depth++;}
                        if (tmp_quote_depth == quote_depth)
                        {
                            if ((tmp_quote_depth>0 || flowed_mail>=1) && *content_ptr==' ' && len>0) {content_ptr++; len--;} //Two spaces and DelSp=yes (flowed_mail==2) should not be handled here
                        }
                        else //New quote-depth. Link should not go across linebreak. Restore values
                        {
                            len = tmp_len;
                            content_ptr = (uni_char*)tmp_linebreak;
                        }
                    }
				}

				if (!in_rfc1738_url)
				{
					//Remove ".,?!" from end of URL, it is probably part of text content
					uni_char url_end_hack;
					while ((url_end_hack=*(content_ptr-1))=='.' ||
						   url_end_hack==',' ||
						   url_end_hack=='?' ||
						   url_end_hack=='!' ||
						   (url_end_hack==')' && !has_left_paranthese))
					{
						content_ptr--;
						len++;
						target[target_used--] = 0;
					}
				}

				RETURN_IF_ERROR(HTMLify_GrowStringIfNeeded(target, target_capacity, target_used, 2));
				uni_strcpy(target+target_used, UNI_L("\">"));
				target_used += 2;

                uni_char* linebreak = NULL; //Points to last linebreak in a multiline URL
                while (urlstart < content_ptr)
				{
					if(*urlstart == '<')
					{
						RETURN_IF_ERROR(HTMLify_GrowStringIfNeeded(target, target_capacity, target_used, 4));
						uni_strcpy(target+target_used, UNI_L("&lt;"));
						target_used += 4;
					}
					else if(*urlstart == '&')
					{
						RETURN_IF_ERROR(HTMLify_GrowStringIfNeeded(target, target_capacity, target_used, 5));
						uni_strcpy(target+target_used, UNI_L("&amp;"));
						target_used += 5;
					}
					else if(*urlstart == '"')
					{
						RETURN_IF_ERROR(HTMLify_GrowStringIfNeeded(target, target_capacity, target_used, 6));
						uni_strcpy(target+target_used, UNI_L("&quot;"));
						target_used += 6;
					}
					else if(((unsigned int) *urlstart) < 256 && ((uni_iscntrl(*urlstart) && !!uni_isspace(*urlstart)) || !op_isprint(*urlstart) || *urlstart == 0x0c || *urlstart == 0x0b))
					{
						RETURN_IF_ERROR(HTMLify_GrowStringIfNeeded(target, target_capacity, target_used, 3));
						target[target_used++] = '%';
						target[target_used++] = UriEscape::EscapeFirst((char)(*urlstart));
						target[target_used++] = UriEscape::EscapeLast((char)(*urlstart));
					}
					else
					{
						RETURN_IF_ERROR(HTMLify_GrowStringIfNeeded(target, target_capacity, target_used, 1));
						target[target_used++] = *urlstart;
					}

                    urlstart++;

                    if ( (in_rfc1738_url && (*urlstart=='\r' || *urlstart=='\n')) ||
                         (flowed_mail>=1 && *urlstart==' ' && (*(urlstart+1)=='\r' || *(urlstart+1)=='\n')) ) //Two spaces and DelSp=yes (flowed_mail==2) should not be handled here
                    {
                        int tmp_quote_depth = 0;
                        if (!(flowed_mail>=1 && *urlstart==' ')) //Linebreaks in flowed content should not be counted
							linebreak = (uni_char*)urlstart;

                        if (*urlstart==' ' && urlstart<content_ptr) urlstart++;
                        if (*urlstart=='\r' && urlstart<content_ptr) urlstart++;
                        if (*urlstart=='\n' && urlstart<content_ptr) urlstart++;
                        while (*urlstart=='>' && urlstart<content_ptr) {urlstart++; tmp_quote_depth++;}
                        if ((tmp_quote_depth>0 || flowed_mail>=1) && *urlstart==' ' && urlstart<content_ptr) urlstart++;
                    }
				}
				RETURN_IF_ERROR(HTMLify_GrowStringIfNeeded(target, target_capacity, target_used, use_xml ? 9 : 4));
				uni_strcpy(target+target_used, (use_xml ? UNI_L("</html:a>") : UNI_L("</a>")));
				target_used += (use_xml ? 9 : 4);

                if (in_rfc1738_url)
                {
					BOOL has_linebreak = FALSE;
                    if (*content_ptr=='\r' && len>0) {linebreak = content_ptr++; len--; has_linebreak=TRUE;}
                    if (*content_ptr=='\n' && len>0) {linebreak = content_ptr++; len--; has_linebreak=TRUE;}
                    while (*content_ptr==' ' && len>0)
					{
						RETURN_IF_ERROR(HTMLify_GrowStringIfNeeded(target, target_capacity, target_used, 1));
						target[target_used++] = *(content_ptr++);
						len--;
					}

                    if (has_linebreak && *content_ptr=='>' && len>3)
					{
						RETURN_IF_ERROR(HTMLify_GrowStringIfNeeded(target, target_capacity, target_used, 4));
						uni_strcpy(target+target_used, UNI_L("&gt;"));
						target_used += 4;
						OP_ASSERT(0); //len -= 4 ?
						len-=4;
						content_ptr++;
					}

					if ( (*content_ptr=='\r' || *content_ptr=='\n') ||
                         (flowed_mail>=1 && *content_ptr==' ' && (*(content_ptr+1)=='\r' || *(content_ptr+1)=='\n')) ) //Two spaces and DelSp=yes (flowed_mail==2) should not be handled here
					{
                        int tmp_quote_depth = 0;
                        linebreak = (uni_char*)content_ptr;
                        if (*content_ptr==' ') {content_ptr++; len--;}
                        if (*content_ptr=='\r') {content_ptr++; len--;}
                        if (*content_ptr=='\n') {content_ptr++; len--;}
                        while (*content_ptr=='>') {content_ptr++; len--; tmp_quote_depth++;}
                        if ((tmp_quote_depth>0 || flowed_mail>=1) && *content_ptr==' ') {content_ptr++; len--;}
					}
                }

                if (linebreak) //We had a linebreak in the URL. Insert linebreak after link-tag
                {
                    int tmp_quote_depth = 0;

                    if (*linebreak==' ') linebreak++;
					RETURN_IF_ERROR(HTMLify_GrowStringIfNeeded(target, target_capacity, target_used, 2));
                    if (*linebreak=='\r') *(target+(target_used++)) = *(linebreak++);
                    if (*linebreak=='\n') *(target+(target_used++)) = *(linebreak++);

                    while (*linebreak=='>')
					{
						RETURN_IF_ERROR(HTMLify_GrowStringIfNeeded(target, target_capacity, target_used, 4));
						uni_strcpy(target+target_used, UNI_L("&gt;"));
						target_used += 4;
						linebreak++;
						tmp_quote_depth++;
					}
                    if ((tmp_quote_depth>0 || flowed_mail>=1) && *linebreak==' ')
					{
						RETURN_IF_ERROR(HTMLify_GrowStringIfNeeded(target, target_capacity, target_used, 1));
						target[target_used++] = *(linebreak++);
					}
                }

				urlstart = content_ptr;
				continue;
			}
		}
		tag = NULL;

#ifdef SMILEY_SUPPORT
		OpString8 smiley_tag;
#endif // SMILEY_SUPPORT

		switch(*content_ptr)
		{
			case '"' :  tag = "&quot;";
				taglen = 6;
				break;
			case '&' :  tag = "&amp;";
				taglen = 5;
				break;
			case '<' :  tag = "&lt;";
				taglen = 4;
				break;
			case '>' :  tag = "&gt;";
				taglen = 4;
				break;
			default:
#ifdef SMILEY_SUPPORT
				if (smileys)
				{
					INT32 smiley_len = GetSmiley(content_ptr, smiley_tag, taglen, content, len, use_xml);

					if (smiley_len)
					{
						smiley_len--;
						content_ptr += smiley_len;
						len -= smiley_len;
						tag = smiley_tag.CStr();
					}
				}
#endif // SMILEY_SUPPORT
				break;
		}

		if(tag)
		{
			RETURN_IF_ERROR(HTMLify_GrowStringIfNeeded(target, target_capacity, target_used, taglen));
			uni_char *copy_dest = target+target_used;
			target_used += taglen;
			for (int i=taglen; i; i--)
			{
				*copy_dest++ = *tag++;
			}
		}
		else if(*content_ptr < 128 &&
			(!uni_isspace(*content_ptr) || *content_ptr == 0x0c || *content_ptr == 0x0b) && (uni_iscntrl(*content_ptr)   || !op_isprint(*content_ptr)) )
		{
			RETURN_IF_ERROR(HTMLify_GrowStringIfNeeded(target, target_capacity, target_used, 3));
			target[target_used++] = '%';
			target[target_used++] = UriEscape::EscapeFirst((char)(*content_ptr));
			target[target_used++] = UriEscape::EscapeLast((char)(*content_ptr));
		}
		else
        {
            if (*content_ptr=='\r' || *content_ptr=='\n')
            {
                quote_depth = 1; //For optimization, add one extra here, and remove it again after loop
                while (quote_depth<len && content_ptr[quote_depth]=='>') quote_depth++;
                quote_depth--;
            }
			RETURN_IF_ERROR(HTMLify_GrowStringIfNeeded(target, target_capacity, target_used, 1));
			target[target_used++] = *content_ptr;
        }

		content_ptr++;
		//if(!isalnum(temp0) && temp0 != '+' && temp0 != '-' && temp0 != '.')
		if(!uni_isalnum(temp0) && temp0 != '+' && temp0 != '-' && temp0 != '.') // 01/04/98 YNP
		{
			urlstart = content_ptr;
		}

		len --;
	}

	target[target_used] = 0;

	return OpStatus::OK;
}

uni_char *XHTMLify_string(const uni_char *content, int len, BOOL no_link, BOOL use_xml, int flowed_mail, BOOL smileys) //flowed_mail: 0:Not flowed, 1:Flowed, 2:Flowed,DelSp=Yes
{
	uni_char* target;
	if (HTMLify_DoEscape(target, content, len, no_link, use_xml, flowed_mail, smileys) != OpStatus::OK)
		return NULL;

	return target;
}

OP_STATUS HTMLify_string(OpString &out_str, const uni_char *content, int len, BOOL no_link, BOOL use_xml, int flowed_mail, BOOL smileys) //flowed_mail: 0:Not flowed, 1:Flowed, 2:Flowed,DelSp=Yes
{
	uni_char* temp_target;
	OP_STATUS ret;
	if ((ret=HTMLify_DoEscape(temp_target, content, len, no_link, use_xml, flowed_mail, smileys)) != OpStatus::OK)
	{
		out_str.Empty();
		return ret;
	}

	ret = out_str.Set(temp_target);
	OP_DELETEA(temp_target);
	return ret;
}

OpString XHTMLify_string_with_BR(const uni_char *content, int len, BOOL no_link, BOOL use_xml, int flowed_mail, BOOL smileys)
{
	OpString result;

	result.TakeOver(XHTMLify_string(content, len, no_link, use_xml, flowed_mail, smileys));

	OpStatus::Ignore(StringUtils::ReplaceNewLines(result));

	return result;
}


OP_STATUS AppendHTMLified(OpString& target, const uni_char* s)
{
	if (!s)
		return OpStatus::ERR;

	uni_char* htmlified = HTMLify_string(s, uni_strlen(s), TRUE);
	if (!htmlified)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status = target.Append(htmlified);
	delete[] htmlified;

	return status;
}
