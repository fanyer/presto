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

#include "modules/formats/encoder.h"
#include "modules/formats/base64_decode.h"

const char Base64_Encoding_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";

#ifdef URL_UPLOAD_QP_SUPPORT
const char g_hexchars[] = "0123456789ABCDEF";

const char *GetHexChars()
{
	return g_hexchars;
}
#endif

#if defined FORMATS_BASE64_CHAR_SUPPORT
const char *GetBase64_Encoding_chars()
{
	return Base64_Encoding_chars;
}
#endif

#if defined URL_UPLOAD_MAILENCODING_SUPPORT || defined URL_UPLOAD_QP_SUPPORT || defined URL_UPLOAD_BASE64_SUPPORT  || defined URL_UPLOAD_HTTP_SUPPORT
#if defined URL_UPLOAD_QP_SUPPORT
BOOL NeedQPEscape(const uni_char c, unsigned flags)
{
	if( (c >= 33 && c<=60) || c== 62 || (c == 63 && (flags & MIME_NOT_CLEANSPACE)==0 ) ||
		(c>=64	 && c<=126 && c != '_'))
		return FALSE;

	if(c == ' ' || c == '_')
	{
		return ((flags & MIME_NOT_CLEANSPACE)!=0 ? TRUE : FALSE);
	}

	if((flags & MIME_IN_COMMENT) != 0)
	{
		//This can't happen because of "(c>=33 && c<=60)" above. '('=40, ')'=41, '"'=34
		//if(c == '(' || 
		//	c == ')' || 
		//	c == '"')
		//	return FALSE;
	}
	else
	{
		if(c == 9 || c== 10 || c==13)
			return FALSE;
	}

	if((flags & MIME_8BIT_RESTRICTION) != 0 && c >=128)
		return FALSE;

	return TRUE;

	/*
	return !(
				(c >= 33 && c<=60) || 
				(c>=62 && c<=126 && c != '_') || 
				(!(flags & MIME_NOT_CLEANSPACE) && 
					c == ' '
				) ||
				(!(flags & MIME_NOT_CLEANSPACE) && 
					c != '_'
				) ||
				(
					(flags & MIME_IN_COMMENT) && 
					(c == '(' || c == ')' || 
						c == '"')
				) ||
				(
					(c == 9 || c== 10 || c==13) && 
					!(flags & MIME_IN_COMMENT)
				) 
			);
			*/
}

int CountQPEscapes(const char* str, int len)
{
    int count = 0;
    for (int i=0; i<len; i++)
    {
        if (NeedQPEscape(str[i],MIME_NOT_CLEANSPACE))
            count++;
    }
    return count;
}
#endif

#if 0 // def URL_UPLOAD_QP_SUPPORT
int CountQPEscapes(const uni_char* str, int len)
{
    int count = 0;
    for (int i=0; i<len; i++)
    {
        if (NeedQPEscape(str[i],MIME_NOT_CLEANSPACE))
            count++;
    }
    return count;
}
#endif


#ifdef URL_UPLOAD_BASE64_SUPPORT
MIME_Encode_Error MIME_Encode_SetStr_Base64(char *&target, int &targetlen, const char *source, int sourcelen, 
		Mime_EncodeTypes encoding)
{
	int linelen = (encoding == GEN_BASE64_ONELINE ? 0 : 48);
    int len,len1;
	char *pos1;
	unsigned char tempchar1, tempchar2, tempchar3;
	unsigned char outcode1, outcode2, outcode3, outcode4;
    
	targetlen =  ((sourcelen +2)/3)*4 + 
		(linelen ? (sourcelen / linelen)*(encoding != UUENCODE ? 2 : 3) : 0 ) + 
		(encoding != UUENCODE ? 0 : 6); 
		
    if(sourcelen ==0 && encoding == UUENCODE)
    	targetlen = 3;
    
	target = OP_NEWA(char, targetlen+32);	// This have to be padded by minumum 32 or else dynamic memory overrun can happen
	if(target == NULL)
		return MIME_FAILURE;
    if(sourcelen ==0 && encoding == UUENCODE)
    {
		op_strcpy(target,"\x60\r\n");
		return MIME_NO_ERROR;
	}
    
	
	len1 = sourcelen;
	len = 0;
	for(pos1 = target;len1 > 0;)
	{
		if(len == 0 && encoding == UUENCODE)
			*(pos1++) = (char) (0x20 + (len1 < linelen ? len1 : 45));
		tempchar1 = *(source++);
		tempchar2 = (len1 > 1 ? *(source++) : 0);
		tempchar3 = (len1 > 2 ? *(source++) : 0);

		outcode1 = (tempchar1 >> 2) & 0x3f;
		outcode2 = (((tempchar1 << 4) & 0x30) | ((tempchar2 >> 4) & 0x0f));
		outcode3 = (len1 > 1 ? (((tempchar2 << 2) & 0x3c) | ((tempchar3 >> 6) & 0x03)) : 64);
		outcode4 = (len1 > 2 ? (tempchar3 & 0x3f) : 64);
		
		len1 = (len1 > 2 ? len1 - 3 : 0);
		
		if(encoding != UUENCODE)
		{
			*(pos1++) = Base64_Encoding_chars[(size_t)outcode1];
			*(pos1++) = Base64_Encoding_chars[(size_t)outcode2];
			*(pos1++) = Base64_Encoding_chars[(size_t)outcode3];
			*(pos1++) = Base64_Encoding_chars[(size_t)outcode4];
		}
		else
		{
			*(pos1++) = (char) (0x20 + (outcode1 ? outcode1 : 64));
			*(pos1++) = (char) (0x20 + (outcode2 ? outcode2 : 64));
			*(pos1++) = (char) (0x20 + (outcode3 ? outcode3 : 64));
			*(pos1++) = (char) (0x20 + (outcode4 ? outcode4 : 64));
		}
		len += 3;
		if(linelen && len>= linelen)
		{
			if(encoding != GEN_BASE64_ONLY_LF)
				*(pos1++) = '\r';
			*(pos1++) = '\n';
			len = 0;
		}
	}
	if(encoding == UUENCODE)
		op_strcpy(pos1,"\r\n\x60\r\n");
	else
		*pos1 = '\0';

	targetlen = (int)op_strlen(target);

	return MIME_NO_ERROR;
}
#endif

#ifdef URL_UPLOAD_HTTP_SUPPORT
MIME_Encode_Error MIME_Encode_SetStr_HTTP(char *&target, int &targetlen, const char *source, int sourcelen)
{
	targetlen =  sourcelen;
	target = OP_NEWA(char, targetlen+1);
	if(target == NULL)
		return MIME_FAILURE;

	op_memcpy(target,source, sourcelen);
	target[targetlen] = '\0';

	return MIME_NO_ERROR;
}
#endif

#ifdef FORMATS_MIME_QP_SUPPORT
MIME_Encode_Error MIME_Encode_SetStr_QuotedPrintable(char *&target, int &targetlen, const char *source, int sourcelen, 
		Mime_EncodeTypes encoding)
{
	int len1,len=0;
	char *pos1;
	unsigned char tempchar;

    len1 = sourcelen;
	const char *pos2 = source;
	for(;len1 > 0;len1--,pos2++)
	{
        len += ( (*pos2==' ' && (*(pos2+1)=='\r' || *(pos2+1)=='\n')) || //Space before linebreak needs to be escaped
                 NeedQPEscape(*pos2, (encoding == MAIL_QP_E ? MIME_NOT_CLEANSPACE : 0)) ? 3 : 1);
	}

	targetlen =  len;
	target = OP_NEWA(char, targetlen+1);
	if(target == NULL)
		return MIME_FAILURE;

    len1 = sourcelen;

	const char *hexchars = GetHexChars();

	for(pos1 = target;len1 > 0;len1--)
	{
		tempchar = *(source++);
		if( (tempchar==' ' && (*source=='\r' || *source=='\n')) ||
            NeedQPEscape(tempchar, (encoding == MAIL_QP_E ? MIME_NOT_CLEANSPACE : 0)) )
		{
			*(pos1++) = '=';
			*(pos1++) = hexchars[(tempchar>>4) & 0x0f];
			*(pos1++) = hexchars[tempchar & 0x0f];
		}
		else
		{
			*(pos1++) = tempchar;
		}
	}
	*pos1 = '\0';
	return MIME_NO_ERROR;
}
#endif

MIME_Encode_Error MIME_Encode_SetStr(char *&target, int &targetlen, const char *source, int sourcelen, 
		const char *charset, Mime_EncodeTypes encoding)
{
#ifdef URL_UPLOAD_MAILENCODING_SUPPORT
	MIME_Encode_Error default_fail;
	int linelen,addcrlf,i;
	const char *pos;
	char temp, *pos1;
#endif
	
	if(target != NULL)
	{
		OP_DELETEA(target);
		target = NULL;
	}
	targetlen = 0;
	if((source == NULL || sourcelen == 0) && encoding != UUENCODE)
		return MIME_NO_ERROR;
	
#ifdef URL_UPLOAD_MAILENCODING_SUPPORT
	BOOL check7bit = FALSE;
#endif
	switch(encoding)
	{
#ifdef URL_UPLOAD_QP_SUPPORT
#if defined  _DEBUG || defined FORMATS_MIME_QP_SUPPORT
		case MAIL_QP    : 
		case MAIL_QP_E  :
#ifdef FORMATS_MIME_QP_SUPPORT
				return MIME_Encode_SetStr_QuotedPrintable(target,targetlen,source,sourcelen,encoding);
#else
				OP_ASSERT(!"API_FORMATS_MIME_QP_SUPPORT must be imported by caller");
#endif 
#endif 
#endif
#ifdef URL_UPLOAD_HTTP_SUPPORT
		case HTTP_ENCODING :
				return MIME_Encode_SetStr_HTTP(target,targetlen,source,sourcelen);
#endif
#ifdef URL_UPLOAD_BASE64_SUPPORT
		case GEN_BASE64 :
		case GEN_BASE64_ONELINE :
		case GEN_BASE64_ONLY_LF:
		case MAIL_BASE64:
		case UUENCODE   :
				return MIME_Encode_SetStr_Base64(target,targetlen,source,sourcelen,encoding);
#endif
		case BINHEX     :
				//return Encode_SetStr_BinHex(target,targetlen,source,sourcelen);
				break;
#ifdef URL_UPLOAD_MAILENCODING_SUPPORT
		case MAIL_7BIT  :
				check7bit = TRUE;
				// fall-through
		case MAIL_8BIT  :
				linelen = 0;
				addcrlf = 0;
				default_fail = (encoding == MAIL_8BIT ? MIME_8BIT_FAIL : MIME_7BIT_FAIL);
				for(i=sourcelen,pos =source; i > 0; i--)
				{
					temp = *(pos++);
					if(!temp || (check7bit && ((unsigned int)temp)>127))   
						return default_fail;
					if(temp == '\r')
					{
						linelen = 0;
						if(i<=1)
							addcrlf ++;
						else
						{
							if(*pos == '\n')
							{
								pos ++;
								i--;
							}
							else
								addcrlf++;
						}
					}
					linelen ++;
					if(linelen > 998)
						return default_fail;
				}
				if(addcrlf>0)
				{
					target = OP_NEWA(char, sourcelen+addcrlf);
					if(target == NULL)
						return MIME_FAILURE;
					
					for(i=sourcelen,pos =source,pos1 =target; i > 0; i--)
					{
						temp = *(pos++);
						if(temp == '\r')
						{
							*(pos1++) = temp;
							temp = '\n';
							if(i > 1 && *pos == '\n')
							{
								pos ++;
								i--;
							}
						} else
							if(temp == '\n')
								*(pos1++) = '\r';
						*(pos1++) = temp;
					}

					targetlen = sourcelen+addcrlf;
					break;
				}
				// fall-through
#endif
#ifdef URL_UPLOAD_HTTP_SUPPORT
		case HTTP_ENCODING_BINARY :
#endif
		case MAIL_BINARY :
		case NO_ENCODING :
		default : 
				target = OP_NEWA(char, sourcelen);
				if(target ==NULL)
					return MIME_FAILURE;
				
				op_memcpy(target, source, sourcelen);
				targetlen = sourcelen;
				break;
	}
	
	return MIME_NO_ERROR;

}
#endif

