/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/util/htmlify.h"
#include "modules/url/url_man.h"
#include "modules/logdoc/htm_lex.h"
#include "modules/formats/uri_escape.h"

char *XMLify_string(const char *content, int len, BOOL no_link, BOOL flowed_mail)
{
	return XHTMLify_string(content, len, no_link, TRUE, flowed_mail);
}

char *HTMLify_string(const char *content, int len, BOOL no_link)
{
	return XHTMLify_string(content, len, no_link, FALSE, FALSE);
}

//Note: return value can be NULL due to OOM or other problems
char *XHTMLify_string(const char *content, int len, BOOL no_link, BOOL use_xml, BOOL flowed_mail)
{
	if(content == NULL)
		return NULL;

	const char *temp1 = content;

    int extralen = 0;
    int len1 = len;
	char temp0;
	const char *urlstart = temp1;
	while(len1> 0)
	{
		len1-- ;

		temp0 = *temp1;

		if(!no_link && temp0 == ':')
		{

			*((char *)temp1) = '\0';
			int scheme_len = op_strlen(urlstart);
			URLType url_type = urlManager->MapUrlType(urlstart);
			*((char *)temp1) = ':';

			char* start_of_url = (char*)(temp1+1);
			if (flowed_mail && *start_of_url==' ' && (*(start_of_url+1)=='\r' || *(start_of_url+1)=='\n'))
			{
				start_of_url++;
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
				extralen += scheme_len + scheme_len + (use_xml ? 28 : 16)*2; //     <A href=""></A> // uri
				//while (len1>0 && !op_isspace(*temp1) &&
				while (len1>0 && !op_isspace((unsigned char) *temp1) && // 01/04/98 YNP
					*temp1 != '"' && *temp1 != '\'' && *temp1 != '>')
				{
					extralen+=2;
	
					if(*temp1 == '<')
						extralen+=6; // "&lt;"*2-2
					else if(*temp1 == '&')
						extralen+=8; // "&amp;"*2-2
					else if(op_iscntrl((unsigned char) *temp1))
						extralen+=4; // "%xx"*2-2
					len1--;
					temp1++;

                    if (flowed_mail && *temp1==' ' && len1>1 && (*(temp1+1)=='\r' || *(temp1+1)=='\n'))
                    {
                        temp1++;
                        if (*temp1=='\r') temp1++;
                        if (*temp1=='\n') temp1++;
                        while (*temp1=='>') temp1++;
                        if (*temp1==' ') temp1++;
                    }
				}
				urlstart = temp1;
				continue;
			}
		}
	
		//if(!op_isalnum(temp0) && temp0 != '+' && temp0 != '-' && temp0 != '.')
		if(!op_isalnum((unsigned char) temp0) && temp0 != '+' && temp0 != '-' && temp0 != '.') // 01/04/98 YNP
			urlstart = temp1+1;

		if(op_iscntrl((unsigned char) temp0) && !op_isspace((unsigned char) temp0) && (((unsigned char)temp0) < 128) )
			extralen+=2;
		else
			switch(temp0)
		{
			case '"' :  extralen += 5;
				break;
			case '\'' :  extralen += 4;
				break;
			case '&' :  extralen += 4;
				break;
			case '<' :  extralen += 3;
				break;
			case '>' :  extralen += 3;
				break;
			case 10  :
				if(use_xml)
					extralen += 14; // assume <p><l></l></p> around each line
				break;
		}
		temp1++;
	}

	char *target = OP_NEWA(char, len + extralen +20);
	if (!target) return NULL;
	const char *tag;
	int taglen = 0;

    char *temp = target;
	char *urlstart_temp = target;
	urlstart = content;
    char* content_ptr = (char*)content;

    int quote_depth = 0;
    while (quote_depth<len && *(content_ptr+quote_depth)=='>') quote_depth++;

	/*
	BOOL first_on_line = TRUE;

	op_strcpy(temp,"<p>");
	temp+=3;
	*/

	while(len > 0)
	{
		temp0 = *content_ptr;

		if(!no_link && temp0 == ':')
		{
			*((char *)content_ptr) = '\0';
			int scheme_len = op_strlen(urlstart);
			URLType url_type = urlManager->MapUrlType(urlstart);
			*((char *)content_ptr) = ':';

			char* start_of_url = (char*)(content_ptr+1);
			if (flowed_mail && *start_of_url==' ' && (*(start_of_url+1)=='\r' || *(start_of_url+1)=='\n'))
			{
				start_of_url++;
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
                temp = (char*)urlstart;
                while (temp>content && *(temp-1)==' ') temp--;
                if ((temp-5)>=content && op_strnicmp(temp-5, "<URL:", 5)==0)
                    in_rfc1738_url = TRUE;

				temp = urlstart_temp;
				op_strcpy(temp,(use_xml ? "<html:a href=\"": "<a href=\""));
				temp += (use_xml ? 9+5 : 9);
				op_strncpy(temp,urlstart,scheme_len);
				temp += scheme_len;

				//while (len>0 && !op_isspace(*content_ptr) &&
				while (len>0 && !op_isspace((unsigned char) *content_ptr) && // 01/04/98 YNP
					*content_ptr != '"' && *content_ptr != '>')
				{
					if(*content_ptr == '<')
					{
						op_strcpy(temp,"&lt;");
						temp+=4;
					}
					else if(*content_ptr == '&')
					{
						op_strcpy(temp,"&amp;");
						temp+=5;
					}
					else if(*content_ptr == '\'')
					{
						op_strcpy(temp,"&#39;");
						temp+=5;
					}
					else
						temp += UriEscape::EscapeIfNeeded(temp, *content_ptr, UriEscape::UniCtrl);

					if (*content_ptr == '(')
						has_left_paranthese = TRUE;
	
					content_ptr++;
					len--;

                    if ( (in_rfc1738_url && (*content_ptr=='\r' || *content_ptr=='\n')) ||
                         (flowed_mail && *content_ptr==' ' && len>1 && (*(content_ptr+1)=='\r' || *(content_ptr+1)=='\n')) )
                    {
                        const char* tmp_linebreak = content_ptr;
                        int tmp_len = len;
                        int tmp_quote_depth = 0;

                        if (*content_ptr==' ' && len>0) {content_ptr++; len--;}
                        if (*content_ptr=='\r' && len>0) {content_ptr++; len--;}
                        if (*content_ptr=='\n' && len>0) {content_ptr++; len--;}
                        while (*content_ptr=='>' && len>0) {content_ptr++; len--; tmp_quote_depth++;}
                        if (tmp_quote_depth == quote_depth)
                        {
                            if ((tmp_quote_depth>0 || flowed_mail) && *content_ptr==' ' && len>0) {content_ptr++; len--;}
                        }
                        else //New quote-depth. Link should not go across linebreak. Restore values
                        {
                            len = tmp_len;
                            content_ptr = (char*)tmp_linebreak;
                        }
                    }
				}
	
				if (!in_rfc1738_url)
				{
					//Remove ".,?!" from end of URL, it is probably part of text content
					char url_end_hack;
					while ((url_end_hack=*(content_ptr-1))=='.' ||
						   url_end_hack==',' ||
						   url_end_hack=='?' ||
						   url_end_hack=='!' ||
						   (url_end_hack==')' && !has_left_paranthese))
					{
						content_ptr--;
						len++;
						temp--;
					}
				}

				op_strcpy(temp,"\">");
				temp += 2;

                char* linebreak = NULL; //Points to last linebreak in a multiline URL
                while (urlstart < content_ptr)
				{
					if(*urlstart == '<')
					{
						op_strcpy(temp,"&lt;");
						temp+=4;
					}
					else if(*urlstart == '&')
					{
						op_strcpy(temp,"&amp;");
						temp+=5;
					}
					else if(*urlstart == '"')
					{
						op_strcpy(temp,"&quot;");
						temp+=6;
					}
					else if(*urlstart == '\'')
					{
						op_strcpy(temp,"&#39;");
						temp+=5;
					}
					else
						temp += UriEscape::EscapeIfNeeded(temp, *urlstart, UriEscape::UniCtrl);

                    urlstart++;

                    if ( (in_rfc1738_url && (*urlstart=='\r' || *urlstart=='\n')) ||
                         (flowed_mail && *urlstart==' ' && (*(urlstart+1)=='\r' || *(urlstart+1)=='\n')) )
                    {
                        int tmp_quote_depth = 0;
                        if (!(flowed_mail && *urlstart==' ')) //Linebreaks in flowed content should not be counted
							linebreak = (char*)urlstart;

                        if (*urlstart==' ' && urlstart<content_ptr) urlstart++;
                        if (*urlstart=='\r' && urlstart<content_ptr) urlstart++;
                        if (*urlstart=='\n' && urlstart<content_ptr) urlstart++;
                        while (*urlstart=='>' && urlstart<content_ptr) {urlstart++; tmp_quote_depth++;}
                        if ((tmp_quote_depth>0 || flowed_mail) && *urlstart==' ' && urlstart<content_ptr) urlstart++;
                    }
				}
				op_strcpy(temp,(use_xml ? "</html:a>" : "</a>"));
				temp += (use_xml ? 4+5 : 4);

                if (in_rfc1738_url)
                {
					BOOL has_linebreak = FALSE;
                    if (*content_ptr=='\r' && len>0) {linebreak = content_ptr++; len--; has_linebreak=TRUE;}
                    if (*content_ptr=='\n' && len>0) {linebreak = content_ptr++; len--; has_linebreak=TRUE;}
                    while (*content_ptr==' ' && len>0) {*(temp++) = *(content_ptr++);  len--;}
                    if (has_linebreak && *content_ptr=='>' && len>3) {op_strcpy(temp, "&gt;"); temp+=4; len-=4; content_ptr++;}

                    if ( (*content_ptr=='\r' || *content_ptr=='\n') ||
                         (flowed_mail && *content_ptr==' ' && (*(content_ptr+1)=='\r' || *(content_ptr+1)=='\n')) )
                    {
                        int tmp_quote_depth = 0;
                        linebreak = (char*)content_ptr;
                        if (*content_ptr==' ') {content_ptr++; len--;}
                        if (*content_ptr=='\r') {content_ptr++; len--;}
                        if (*content_ptr=='\n') {content_ptr++; len--;}
                        while (*content_ptr=='>') {content_ptr++; len--; tmp_quote_depth++;}
                        if ((tmp_quote_depth>0 || flowed_mail) && *content_ptr==' ') {content_ptr++; len--;}
                    }
                }

                if (linebreak) //We had a linebreak in the URL. Insert linebreak after link-tag
                {
                    int tmp_quote_depth = 0;
                    if (*linebreak==' ') linebreak++;
                    if (*linebreak=='\r') *(temp++) = *(linebreak++);
                    if (*linebreak=='\n') *(temp++) = *(linebreak++);
                    while (*linebreak=='>') {op_strcpy(temp, "&gt;"); temp+=4; linebreak++; tmp_quote_depth++;}
                    if ((tmp_quote_depth>0 || flowed_mail) && *linebreak==' ') *(temp++) = *(linebreak++);
                }

				urlstart = content_ptr;
				urlstart_temp = temp;
				continue;
			}
		}
		tag = NULL;
		switch(*content_ptr)
		{
			case '"' :  tag = "&quot;";
				taglen = 6;
				break;
			case '\'' :  tag = "&#39;";
				taglen = 5;
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
		}

		if(tag)
		{
			op_strcpy(temp,tag);
			temp+=taglen;
		}
		else
        {
            if (*content_ptr=='\r' || *content_ptr=='\n')
            {
                quote_depth = 1; //For optimization, add one extra here, and remove it again after loop
                while (quote_depth<len && *(content_ptr+quote_depth)=='>') quote_depth++;
                quote_depth--;
            }
			temp += UriEscape::EscapeIfNeeded(temp, *content_ptr,
				UriEscape::PrefixBackslash | UriEscape::NonSpaceCtrl);
        }

        content_ptr++;
		//if(!iop_salnum(temp0) && temp0 != '+' && temp0 != '-' && temp0 != '.')
		if(!op_isalnum((unsigned char) temp0) && temp0 != '+' && temp0 != '-' && temp0 != '.') // 01/04/98 YNP
		{
			urlstart = content_ptr;
			urlstart_temp = temp;
		}

		len --;
	}
	*temp = '\0';

	return target;
}

uni_char *HTMLify_string_link(const uni_char *content)
{
	if(content == NULL)
		return NULL;

	return XHTMLify_string(content,uni_strlen(content), FALSE, FALSE, FALSE);
}

uni_char *HTMLify_string(const uni_char *content) // No link
{
	if(content == NULL)
		return NULL;

	return XHTMLify_string(content,uni_strlen(content), TRUE, FALSE, FALSE);
}

uni_char *XMLify_string(const uni_char *content) // No link
{
	if(content == NULL)
		return NULL;

	return XHTMLify_string(content,uni_strlen(content), TRUE, TRUE, FALSE);
}

uni_char *XMLify_string(const uni_char *content, int len, BOOL no_link, BOOL flowed_mail)
{
	return XHTMLify_string(content, len, no_link, TRUE, flowed_mail);
}

uni_char *HTMLify_string(const uni_char *content, int len, BOOL no_link)
{
	return XHTMLify_string(content, len, no_link, FALSE, FALSE);
}

int		HTMLify_EscapedLen(const uni_char *content, int len, BOOL no_link, BOOL use_xml, BOOL flowed_mail);
void	HTMLify_DoEscape(uni_char *&target, const uni_char *content, int len, BOOL no_link, BOOL use_xml, BOOL flowed_mail);

int HTMLify_EscapedLen(const uni_char *content, int len, BOOL no_link, BOOL use_xml, BOOL flowed_mail)
{
    int extralen = 0;
	const uni_char *temp1 = content;
    int len1 = len;
	uni_char temp0;
	const uni_char *urlstart = temp1;

	while(len1> 0)
	{
		len1--;

		temp0 = *temp1;

		if(!no_link && temp0 == ':')
		{
			*((uni_char *)temp1) = '\0';
			int scheme_len = uni_strlen(urlstart);
			URLType url_type = urlManager->MapUrlType(urlstart);
			*((uni_char *)temp1) = ':';

			uni_char* start_of_url = (uni_char*)(temp1+1);
			if (flowed_mail && *start_of_url==' ' && (*(start_of_url+1)=='\r' || *(start_of_url+1)=='\n'))
			{
				start_of_url++;
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
				extralen += scheme_len + scheme_len + (use_xml ? 28 : 16); //     <A href=""></A> // uri
				//while (len1>0 && !op_isspace(*temp1) &&
				while (len1>0 && !( ((unsigned int) *temp1) < 256 && uni_isspace(*temp1)) &&
					*temp1 != '"' && *temp1 != '\'' && *temp1 != '>')
				{
					extralen+=2;
	
					if(*temp1 == '<')
						extralen+=6;
					else if(*temp1 == '&')
						extralen+=8;
					else if(((unsigned int) *temp1) < 256 && uni_iscntrl(*temp1))
						extralen+=4;
					len1--;
					temp1++;

                    if (flowed_mail && *temp1==' ' && len1>1 && (*(temp1+1)=='\r' || *(temp1+1)=='\n'))
                    {
                        temp1++;
                        if (*temp1=='\r') temp1++;
                        if (*temp1=='\n') temp1++;
                        while (*temp1=='>') temp1++;
                        if (*temp1==' ') temp1++;
                    }
				}
				urlstart = temp1;
				continue;
			}
		}
	
		if(!(((unsigned int) temp0) < 256 && uni_isalnum(temp0)) && temp0 != '+' && temp0 != '-' && temp0 != '.') // 01/04/98 YNP
			urlstart = temp1+1;

		if(temp0 == 0 || temp0 == 0x0b ||
			( ((unsigned int) temp0) < 128 &&
				((uni_iscntrl(temp0) && (!uni_isspace(temp0) || temp0 == 0x0c)) ||
					!op_isprint(temp0))))
			extralen+=2;
		else
			switch(temp0)
		{
			case '"' :  extralen += 5;
				break;
			case '\'' :  extralen += 4;
				break;
			case '&' :  extralen += 4;
				break;
			case '<' :  extralen += 3;
				break;
			case '>' :  extralen += 3;
				break;
			case 10  :
				if(use_xml)
					extralen += 14; // assume <p><l></l></p> around each line
				break;
		}
		temp1++;
	}

	return extralen + len;
}

void HTMLify_DoEscape(uni_char *&target, const uni_char *content, int len, BOOL no_link, BOOL use_xml, BOOL flowed_mail)
{
	const uni_char *tag;
	int taglen = 0;

	uni_char temp0;
    uni_char *temp = target;
	uni_char *urlstart_temp = target;
	const uni_char *urlstart = content;
    uni_char* content_ptr = (uni_char*)content;

    int quote_depth = 0;
    while (quote_depth<len && *(content_ptr+quote_depth)=='>') quote_depth++;

	while(len > 0)
	{
		temp0 = *content_ptr;

		if(!no_link && temp0 == ':')
		{
			*((uni_char *)content_ptr) = '\0';
			int scheme_len = uni_strlen(urlstart);
			URLType url_type = urlManager->MapUrlType(urlstart);
			*((uni_char *)content_ptr) = ':';

			uni_char* start_of_url = (uni_char*)(content_ptr+1);
			if (flowed_mail && *start_of_url==' ' && (*(start_of_url+1)=='\r' || *(start_of_url+1)=='\n'))
			{
				start_of_url++;
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
                temp = (uni_char*)urlstart;
                while (temp>content && *(temp-1)==' ') temp--;
                if ((temp-5)>=content && uni_strni_eq(temp-5, "<URL:", 5))
                    in_rfc1738_url = TRUE;

				temp = urlstart_temp;
				uni_strcpy(temp,(use_xml ? UNI_L("<html:a href=\""): UNI_L("<a href=\"")));
				temp += (use_xml ? 9+5 : 9);
				uni_strncpy(temp,urlstart,scheme_len);
				temp += scheme_len;

				//while (len>0 && !op_isspace(*content_ptr) &&
				while (len>0 && !(((unsigned int) *content_ptr) < 256 && uni_isspace(*content_ptr)) &&
					*content_ptr != '"' && *content_ptr != '>')
				{
					if(*content_ptr == '<')
					{
						uni_strcpy(temp,UNI_L("&lt;"));
						temp+=4;
					}
					else if(*content_ptr == '&')
					{
						uni_strcpy(temp,UNI_L("&amp;"));
						temp+=5;
					}
					else if(*content_ptr == '\'')
					{
						uni_strcpy(temp,UNI_L("&#39;"));
						temp+=5;
					}
					else
						temp += UriEscape::EscapeIfNeeded(temp, *content_ptr, UriEscape::UniCtrl & ~UriEscape::SpaceCtrl);

					if (*content_ptr == '(')
						has_left_paranthese = TRUE;

                    content_ptr++;
					len--;

                    if ( (in_rfc1738_url && (*content_ptr=='\r' || *content_ptr=='\n')) ||
                         (flowed_mail && *content_ptr==' ' && len>1 && (*(content_ptr+1)=='\r' || *(content_ptr+1)=='\n')) )
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
                            if ((tmp_quote_depth>0 || flowed_mail) && *content_ptr==' ' && len>0) {content_ptr++; len--;}
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
						temp--;
					}
				}

				uni_strcpy(temp,UNI_L("\">"));
				temp += 2;

                uni_char* linebreak = NULL; //Points to last linebreak in a multiline URL
                while (urlstart < content_ptr)
				{
					if(*urlstart == '<')
					{
						uni_strcpy(temp,UNI_L("&lt;"));
						temp+=4;
					}
					else if(*urlstart == '&')
					{
						uni_strcpy(temp,UNI_L("&amp;"));
						temp+=5;
					}
					else if(*urlstart == '"')
					{
						uni_strcpy(temp,UNI_L("&quot;"));
						temp+=6;
					}
					else if(*urlstart == '\'')
					{
						uni_strcpy(temp,UNI_L("&#39;"));
						temp+=5;
					}
					else
						temp += UriEscape::EscapeIfNeeded(temp, *urlstart, UriEscape::UniCtrl & ~UriEscape::SpaceCtrl);

                    urlstart++;

                    if ( (in_rfc1738_url && (*urlstart=='\r' || *urlstart=='\n')) ||
                         (flowed_mail && *urlstart==' ' && (*(urlstart+1)=='\r' || *(urlstart+1)=='\n')) )
                    {
                        int tmp_quote_depth = 0;
                        if (!(flowed_mail && *urlstart==' ')) //Linebreaks in flowed content should not be counted
							linebreak = (uni_char*)urlstart;

                        if (*urlstart==' ' && urlstart<content_ptr) urlstart++;
                        if (*urlstart=='\r' && urlstart<content_ptr) urlstart++;
                        if (*urlstart=='\n' && urlstart<content_ptr) urlstart++;
                        while (*urlstart=='>' && urlstart<content_ptr) {urlstart++; tmp_quote_depth++;}
                        if ((tmp_quote_depth>0 || flowed_mail) && *urlstart==' ' && urlstart<content_ptr) urlstart++;
                    }
				}
				uni_strcpy(temp,(use_xml ? UNI_L("</html:a>") : UNI_L("</a>")));
				temp += (use_xml ? 4+5 : 4);

                if (in_rfc1738_url)
                {
					BOOL has_linebreak = FALSE;
                    if (*content_ptr=='\r' && len>0) {linebreak = content_ptr++; len--; has_linebreak=TRUE;}
                    if (*content_ptr=='\n' && len>0) {linebreak = content_ptr++; len--; has_linebreak=TRUE;}
                    while (*content_ptr==' ' && len>0) {*(temp++) = *(content_ptr++);  len--;}
                    if (has_linebreak && *content_ptr=='>' && len>3) {uni_strcpy(temp, UNI_L("&gt;")); temp+=4; len-=4; content_ptr++;}
 
					if ( (*content_ptr=='\r' || *content_ptr=='\n') ||
                         (flowed_mail && *content_ptr==' ' && (*(content_ptr+1)=='\r' || *(content_ptr+1)=='\n')) )
					{
                        int tmp_quote_depth = 0;
                        linebreak = (uni_char*)content_ptr;
                        if (*content_ptr==' ') {content_ptr++; len--;}
                        if (*content_ptr=='\r') {content_ptr++; len--;}
                        if (*content_ptr=='\n') {content_ptr++; len--;}
                        while (*content_ptr=='>') {content_ptr++; len--; tmp_quote_depth++;}
                        if ((tmp_quote_depth>0 || flowed_mail) && *content_ptr==' ') {content_ptr++; len--;}
					}
                }

                if (linebreak) //We had a linebreak in the URL. Insert linebreak after link-tag
                {
                    int tmp_quote_depth = 0;
                    if (*linebreak==' ') linebreak++;
                    if (*linebreak=='\r') *(temp++) = *(linebreak++);
                    if (*linebreak=='\n') *(temp++) = *(linebreak++);
                    while (*linebreak=='>') {uni_strcpy(temp, UNI_L("&gt;")); temp+=4; linebreak++; tmp_quote_depth++;}
                    if ((tmp_quote_depth>0 || flowed_mail) && *linebreak==' ') *(temp++) = *(linebreak++);
                }

				urlstart = content_ptr;
				urlstart_temp = temp;
				continue;
			}
		}
		tag = NULL;
		switch(*content_ptr)
		{
			case '"' :  tag = (uni_char*)UNI_L("&quot;");
				taglen = 6;
				break;
			case '\'' :  tag = (uni_char*)UNI_L("&#39;");
				taglen = 5;
				break;
			case '&' :  tag = (uni_char*)UNI_L("&amp;");
				taglen = 5;
				break;
			case '<' :  tag = (uni_char*)UNI_L("&lt;");
				taglen = 4;
				break;
			case '>' :  tag = (uni_char*)UNI_L("&gt;");
				taglen = 4;
				break;
		}

		if(tag)
		{
			uni_strcpy(temp,tag);
			temp+=taglen;
		}
		else
        {
            if (*content_ptr=='\r' || *content_ptr=='\n')
            {
                quote_depth = 1; //For optimization, add one extra here, and remove it again after loop
                while (quote_depth<len && *(content_ptr+quote_depth)=='>') quote_depth++;
                quote_depth--;
            }
			temp += UriEscape::EscapeIfNeeded(temp, *content_ptr,
				UriEscape::PrefixBackslash | UriEscape::NonSpaceCtrl);
        }

		content_ptr++;
		//if(!op_isalnum(temp0) && temp0 != '+' && temp0 != '-' && temp0 != '.')
		if(!uni_isalnum(temp0) && temp0 != '+' && temp0 != '-' && temp0 != '.') // 01/04/98 YNP
		{
			urlstart = content_ptr;
			urlstart_temp = temp;
		}

		len --;
	}
	*temp = '\0';
}

uni_char *XHTMLify_string(const uni_char *content, int len, BOOL no_link, BOOL use_xml, BOOL flowed_mail)
{
	if(content == NULL)
		return NULL;

	int new_len = HTMLify_EscapedLen(content, len, no_link, use_xml, flowed_mail);
	if (new_len == len)
	{
		uni_char* copy = OP_NEWA(uni_char, new_len + 1);
		if (copy)
		{
			op_memcpy(copy, content, sizeof(uni_char)*new_len);
			copy[new_len] = '\0';
		}
		return copy;
	}

	uni_char *target = OP_NEWA(uni_char, new_len + 20); // "+ 20 just in case" says Yngve
	if (!target) return NULL;

	HTMLify_DoEscape(target, content, len, no_link, use_xml, flowed_mail);
	return target;
}
