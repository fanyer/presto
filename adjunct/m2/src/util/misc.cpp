/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2003-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef M2_SUPPORT

#include "adjunct/m2/src/util/misc.h"

#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/util/qp.h"
#include "modules/auth/auth_digest.h"
#include "modules/idna/idna.h"
#include "modules/libcrypto/libcrypto_public_api.h"
#include "modules/url/url_man.h"

OP_STATUS OpMisc::EncodeKeyword(const OpStringC16& source, OpString8& dest)
{
	if (source.IsEmpty())
	{
		dest.Empty();
		return OpStatus::OK; // empty string
	}

	OpString source_wo_spaces, space, underscore;
	RETURN_IF_ERROR(source_wo_spaces.Set(source.CStr()));
	RETURN_IF_ERROR(space.Set(" "));
	RETURN_IF_ERROR(underscore.Set("_"));
	RETURN_IF_ERROR(StringUtils::Replace(source_wo_spaces, space, underscore));

	static UTF16toUTF7Converter converter(UTF16toUTF7Converter::IMAP);
	int    src_len      = source_wo_spaces.Length();
	int    max_dest_len = (src_len * 4) + 2;
	int    bytes_read;
	OpAutoPtr<char> dest_ptr (OP_NEWA(char, max_dest_len));

	if (!dest_ptr.get())
		return OpStatus::ERR_NO_MEMORY;

	// Encode
	int written = converter.Convert(source_wo_spaces.CStr(), UNICODE_SIZE(src_len), dest_ptr.get(), max_dest_len-1, &bytes_read);

	// Check if we got all
	if (written < 0 || bytes_read < UNICODE_SIZE(src_len))
		return OpStatus::ERR;

	// terminate string
	dest_ptr.get()[written] = '\0';

	return dest.Set(dest_ptr.get());
}

OP_STATUS OpMisc::DecodeMailboxOrKeyword(const OpStringC8& source, OpString& dest)
{
	if (source.IsEmpty())
	{
		dest.Empty();
		return OpStatus::OK; // empty string
	}

	static    UTF7toUTF16Converter converter(UTF7toUTF16Converter::IMAP);
	int       src_len      = source.Length();
	uni_char* dest_ptr     = dest.Reserve(src_len);
	int       bytes_read;

	if (!dest_ptr)
		return OpStatus::ERR_NO_MEMORY;

	// Decode
	int written = converter.Convert(source.CStr(), src_len, dest_ptr, UNICODE_SIZE(src_len), &bytes_read);

	// Check if we got all
	if (written < 0 || bytes_read < src_len)
		return OpStatus::ERR;

	// terminate string
	dest_ptr[UNICODE_DOWNSIZE(written)] = 0;

	return OpStatus::OK;
}

OP_STATUS OpMisc::CalculateMD5Checksum(const char* buffer, int buffer_length, OpString8& md5_checksum)
{
	// code moved here since it's static and we need it without a glue factory for BT to work (blame johan@opera.com)
	OpAutoPtr<CryptoHash> md5(CryptoHash::CreateMD5());
	if (!md5.get())
		return OpStatus::ERR_NO_MEMORY;

	md5->InitHash();
	md5->CalculateHash((const BYTE*)buffer, buffer_length);
	if (md5_checksum.Reserve(33)==NULL)
		return OpStatus::ERR_NO_MEMORY;

	ConvertToHex(md5, md5_checksum.CStr());

	return OpStatus::OK;
}

const uni_char* OpMisc::uni_rfc822_skipCFWS(const uni_char* string, OpString& comment)
{
	if (!string)
		return NULL;

	comment.Empty();
	int comment_level = 0;
	const uni_char* comment_start = NULL;
	uni_char tmp_c;
	while ((tmp_c=*string) != 0)
	{
		if (tmp_c=='(' || (tmp_c==')' && comment_level>0))
		{
			if (tmp_c=='(' && comment_level==0) //Start of comment?
			{
				comment_start = string;
			}

			comment_level += (tmp_c=='(' ? 1 : -1); //Adjust comment ref-count

			if (tmp_c==')' && comment_level==0) //End of comment?
			{
				if (comment.HasContent()) OpStatus::Ignore(comment.Append(UNI_L(" ")));
				OpStatus::Ignore(comment.Append(comment_start, string-comment_start+1));
			}
		}
		else if (comment_level==0)
		{
			if (tmp_c==' ' || tmp_c=='\t')
			{
				//Do nothing
			}
			else if (tmp_c=='\\' && (*(string+1)==' ' || *(string+1)=='\t'))
			{
				string++; //Skip quoted-char. The character itself will be skipped further down
			}
			else if (tmp_c=='\r' && *(string+1)=='\n' && (*(string+2)==' ' || *(string+2)=='\t'))
			{
				string+=2; //Skip CRLF. Whitespace will be skipped further down, and any other whitespace will bi skipped later
			}
			else //We have a non-CFWS-character
			{
				return string;
			}
		}
		string++;
	}
	return string;
}

const uni_char* OpMisc::uni_rfc822_skip_until_CFWS(const uni_char* string)
{
	if (!string)
		return NULL;

	bool in_quote = false;
	uni_char tmp_c;
	while ((tmp_c=*string) != 0)
	{
		if (tmp_c=='\"')
		{
			in_quote = !in_quote;
		}
		else if (tmp_c=='\\')
		{
			string++; //Skip quoted-char. The character itself will be skipped further down

			if (!*string)
				return string;
		}
		else if (!in_quote &&
				 (tmp_c=='(' || //Comment
				  tmp_c==' ' || tmp_c=='\t' || //WSP
				  (tmp_c=='\r' && *(string+1)=='\n' && (*(string+2)==' ' || *(string+2)=='\t'))) ) //FWS
		{
			return string;
		}

		string++;
	}
	return string;
}

const uni_char* OpMisc::uni_rfc822_strchr(const uni_char* string, uni_char c, bool skip_quotes, bool skip_comments)
{
	if (!string)
		return NULL;

	int comment_level = 0;
	bool in_quote = false;
	uni_char tmp_c;
	while ((tmp_c=*string) != 0)
	{
		if (skip_quotes && tmp_c=='\"' && comment_level==0) //Comments can contain quote-char
		{
			in_quote = !in_quote;
		}
		else if (skip_comments && (tmp_c=='(' || tmp_c==')') && !in_quote) //Quoted strings can contain '(' and ')'.
		{
			if (c=='(' && c==tmp_c && comment_level==0) //If we are looking for '(', catch it before quote_level is increased
				return string;

			comment_level += (tmp_c=='(' ? 1 : -1);
			if (comment_level < 0)
				comment_level = 0;
		}
		else if (tmp_c=='\\')
		{
			if ((tmp_c=*(++string))==0) //Skip quoted-char. The character itself will be skipped further down
			{
				return NULL;
			}
		}

		if (comment_level==0 && !in_quote && tmp_c==c) //We have found our character
		{
			return string;
		}
		string++;
	}
	return NULL;
}

const char* OpMisc::rfc822_strchr(const char* string, char c, bool skip_quotes, bool skip_comments)
{
	if (!string)
		return NULL;

	int comment_level = 0;
	bool in_quote = false;
	char tmp_c;
	while ((tmp_c=*string) != 0)
	{
		if (skip_quotes && tmp_c=='\"' && comment_level==0) //Comments can contain quote-char
		{
			in_quote = !in_quote;
		}
		else if (skip_comments && (tmp_c=='(' || tmp_c==')') && !in_quote) //Quoted strings can contain '(' and ')'.
		{
			if (c=='(' && c==tmp_c && comment_level==0) //If we are looking for '(', catch it before quote_level is increased
				return string;

			comment_level += (tmp_c=='(' ? 1 : -1);
			if (comment_level < 0)
				comment_level = 0;
		}
		else if (tmp_c=='\\')
		{
			if ((tmp_c=*(++string))==0) //Skip quoted-char. The character itself will be skipped further down
			{
				return NULL;
			}
		}

		if (comment_level==0 && !in_quote && tmp_c==c) //We have found our character
		{
			return string;
		}
		string++;
	}
	return NULL;
}

OP_STATUS OpMisc::StripQuotes(OpString8& string)
{
	if (string.IsEmpty())
		return OpStatus::OK; //Nothing to do

    const char* start_ptr = string.CStr();
	while (*start_ptr=='"' && *(start_ptr+1)=='"')
	{
		string.Delete(0,2);
		start_ptr = string.CStr();
	}

    char* end_ptr = const_cast<char*>(start_ptr+string.Length()-1);
	if (*start_ptr!='"' || *end_ptr!='"')
		return OpStatus::OK; //Not matching quotes around string

	//Strip quotes
	BOOL strip_quotes = TRUE;
	const char* temp_ptr = start_ptr+1;
	char temp_char;
	while (strip_quotes && temp_ptr<end_ptr && (temp_char=*temp_ptr)!=0)
	{
		if (temp_char=='"') //Not matching quote.
		{
			strip_quotes = FALSE;
		}
		else if (temp_char=='\\') //Quote-pair.
		{
			temp_ptr++; //Skip this char, and the next (will be done further down)
		}

		temp_ptr++;
	}

	if (temp_ptr>end_ptr)
		strip_quotes=FALSE;

	if (strip_quotes)
	{
		*end_ptr = 0;
		string.Delete(0,1);
	}

	return OpStatus::OK;
}

OP_STATUS OpMisc::StripQuotes(OpString16& string)
{
	if (string.IsEmpty())
		return OpStatus::OK; //Nothing to do

    const uni_char* start_ptr = string.CStr();
	while (*start_ptr=='"' && *(start_ptr+1)=='"')
	{
		string.Delete(0,2);
		start_ptr = string.CStr();
	}

    uni_char* end_ptr = const_cast<uni_char*>(start_ptr+string.Length()-1);
	if (*start_ptr!='"' || *end_ptr!='"')
		return OpStatus::OK; //Not matching quotes around string

	//Strip quotes
	BOOL strip_quotes = TRUE;
	const uni_char* temp_ptr = start_ptr+1;
	uni_char temp_char;
	while (strip_quotes && temp_ptr<end_ptr && (temp_char=*temp_ptr)!=0)
	{
		if (temp_char=='"') //Not matching quote.
		{
			strip_quotes = FALSE;
		}
		else if (temp_char=='\\') //Quote-pair.
		{
			temp_ptr++; //Skip this char, and the next (will be done further down)
		}

		temp_ptr++;
	}

	if (temp_ptr>end_ptr)
		strip_quotes=FALSE;

	if (strip_quotes)
	{
		*end_ptr = 0;
		string.Delete(0,1);
	}

	return OpStatus::OK;
}


OP_STATUS OpMisc::StripWhitespaceAndQuotes(OpString16& string, BOOL strip_whitespace, BOOL strip_quotes)
{
	OP_ASSERT(strip_whitespace || strip_quotes);
	OP_ASSERT(strip_quotes); //If you are just going to use strip_whitespace, you might consider using OpStringS16::Strip()

    if (string.IsEmpty())
        return OpStatus::OK; //Nothing to strip

	if (strip_whitespace)
		string.Strip();

	return strip_quotes ? StripQuotes(string) : OpStatus::OK;
}


void OpMisc::StripTrailingLinefeeds(OpString16& string)
{
    uni_char* str_start = string.CStr();
    if (!str_start || !*str_start)
        return; //Nothing to do

    int str_length = uni_strlen(str_start);

    uni_char* str_end = str_start+str_length-1;
    while (str_end>=str_start && (*str_end=='\r' || *str_end=='\n')) --str_end;
    *(str_end+1) = 0;
}

OP_STATUS OpMisc::DecodeQuotePair(OpString8& string)
{
    if (string.IsEmpty() || strchr(string.CStr(), '\\')==NULL) //Nothing to do
        return OpStatus::OK;

    OpString8 decoded;
    if (!decoded.Reserve(string.Length()))
        return OpStatus::ERR_NO_MEMORY;

    OP_STATUS ret;
    const char* start_ptr = string.CStr();
    const char* end_ptr;

	const char* iso2022jp_start = strstr(start_ptr, "\x1B\x24\x42"); //[ESC]$B, RFC1468
	const char* iso2022jp_end = iso2022jp_start ? strstr(iso2022jp_start, "\x01B(B") : NULL; //[ESC](B

    if (*start_ptr == '\\') start_ptr++;
    while (start_ptr)
    {
        end_ptr = strchr(start_ptr+1, '\\');

		//Don't decode within iso-2022-jp (RFC1468) encoded words
		if (iso2022jp_start && end_ptr)
		{
			while (iso2022jp_start &&
				   iso2022jp_end &&
				   end_ptr>iso2022jp_end)
			{
				iso2022jp_start = strstr(iso2022jp_end+1, "\x01B$B"); //[ESC]$B
				iso2022jp_end = iso2022jp_start ? strstr(iso2022jp_start, "\x01B(B") : NULL; //[ESC](B
			}

			while (iso2022jp_start &&
				   end_ptr &&
				   end_ptr>iso2022jp_start &&
				   end_ptr<iso2022jp_end)
			{
				end_ptr = strchr(iso2022jp_end+1, '\\');

				iso2022jp_start = strstr(iso2022jp_end+1, "\x01B$B"); //[ESC]$B
				iso2022jp_end = iso2022jp_start ? strstr(iso2022jp_start, "\x01B(B") : NULL; //[ESC](B
			}
		}

		//Don't decode gross QP-lookalike hack (where '?' is quotepair-encoded by M2)
		while (end_ptr && *(end_ptr+1)=='?' && end_ptr>string.CStr() && *(end_ptr-1)=='=')
		{
			end_ptr = strchr(end_ptr+1, '\\');
		}

        if (!end_ptr)
        {
            if ((ret=decoded.Append(start_ptr)) != OpStatus::OK)
                return ret;

            break;
        }
        else if (start_ptr != end_ptr)
        {
            if ((ret=decoded.Append(start_ptr, end_ptr-start_ptr)) != OpStatus::OK)
                return ret;
        }

        start_ptr = end_ptr+1;
    }

    return string.Set(decoded);
}

OP_STATUS OpMisc::DecodeQuotePair(OpString16& string)
{
    if (string.IsEmpty() || uni_strchr(string.CStr(), '\\')==NULL) //Nothing to do
        return OpStatus::OK;

    OpString decoded;
    if (!decoded.Reserve(string.Length()))
        return OpStatus::ERR_NO_MEMORY;

    OP_STATUS ret;
    uni_char* start_ptr = string.CStr();
    uni_char* end_ptr;

    if (*start_ptr == '\\') start_ptr++;
    while (start_ptr)
    {
        end_ptr = uni_strchr(start_ptr+1, '\\');

		//Don't decode gross QP-lookalike hack (where '?' is quotepair-encoded by M2)
		while (end_ptr && *(end_ptr+1)=='?' && end_ptr>string.CStr() && *(end_ptr-1)=='=')
		{
			end_ptr = uni_strchr(end_ptr+1, '\\');
		}

        if (!end_ptr)
        {
            if ((ret=decoded.Append(start_ptr)) != OpStatus::OK)
                return ret;

            break;
        }
        else if (start_ptr != end_ptr)
        {
            if ((ret=decoded.Append(start_ptr, end_ptr-start_ptr)) != OpStatus::OK)
                return ret;
        }

        start_ptr = end_ptr+1;
    }

    return string.Set(decoded);
}

OP_STATUS OpMisc::EncodeQuotePair(OpString8& string, BOOL encode_dobbeltfnutt)
{
    if (string.IsEmpty() ||
		(strchr(string.CStr(), '\\')==NULL &&
		 (!encode_dobbeltfnutt || strchr(string.CStr(), '\"')==NULL)))
	{
        return OpStatus::OK; //Nothing to do
	}

    OpString8 encoded;
    if (!encoded.Reserve(string.Length()+10)) //If more than 10 quote-chars are present, things will start to run slower (Grow for each Append)
        return OpStatus::ERR_NO_MEMORY;

    OP_STATUS ret;
    char* start_ptr = string.CStr();
    char* end_ptr;
    while (start_ptr)
    {
		if (encode_dobbeltfnutt)
			end_ptr = strpbrk(start_ptr, "\\\"");
		else
			end_ptr = strchr(start_ptr, '\\');

		//Don't encode gross QP-lookalike hack (where '?' is quotepair-encoded by M2)
		while (end_ptr && *(end_ptr+1)=='?' && end_ptr>string.CStr() && *(end_ptr-1)=='=')
		{
			if (encode_dobbeltfnutt)
				end_ptr = strpbrk(end_ptr+1, "\\\"");
			else
				end_ptr = strchr(end_ptr+1, '\\');
		}

        if (!end_ptr)
        {
            if ((ret=encoded.Append(start_ptr)) != OpStatus::OK)
                return ret;

            break;
        }
        else
        {
            if (start_ptr != end_ptr)
            {
                if ((ret=encoded.Append(start_ptr, end_ptr-start_ptr)) != OpStatus::OK)
                    return ret;
            }

            if ((ret=encoded.Append("\\"))!=OpStatus::OK ||
				(ret=encoded.Append(end_ptr, 1))!=OpStatus::OK)
			{
                return ret;
			}
        }

        start_ptr = end_ptr+1;
    }

    return string.Set(encoded);
}

OP_STATUS OpMisc::EncodeQuotePair(OpString16& string, BOOL encode_dobbeltfnutt)
{
    if (string.IsEmpty() ||
		(uni_strchr(string.CStr(), '\\')==NULL &&
		 (!encode_dobbeltfnutt || uni_strchr(string.CStr(), '\"')==NULL)))
	{
        return OpStatus::OK; //Nothing to do
	}

    OpString encoded;
    if (!encoded.Reserve(string.Length()+10)) //If more than 10 quote-chars are present, things will start to run slower (Grow for each Append)
        return OpStatus::ERR_NO_MEMORY;

    OP_STATUS ret;
    uni_char* start_ptr = string.CStr();
    uni_char* end_ptr;
    while (start_ptr)
    {
		if (encode_dobbeltfnutt)
			end_ptr = uni_strpbrk(start_ptr, UNI_L("\\\""));
		else
			end_ptr = uni_strchr(start_ptr, '\\');

		//Don't encode gross QP-lookalike hack (where '?' is quotepair-encoded by M2)
		while (end_ptr && *(end_ptr+1)=='?' && end_ptr>string.CStr() && *(end_ptr-1)=='=')
		{
			if (encode_dobbeltfnutt)
				end_ptr = uni_strpbrk(end_ptr+1, UNI_L("\\\""));
			else
				end_ptr = uni_strchr(end_ptr+1, '\\');
		}

        if (!end_ptr)
        {
            if ((ret=encoded.Append(start_ptr)) != OpStatus::OK)
                return ret;

            break;
        }
        else
        {
            if (start_ptr != end_ptr)
            {
                if ((ret=encoded.Append(start_ptr, end_ptr-start_ptr)) != OpStatus::OK)
                    return ret;
            }

            if ((ret=encoded.Append(UNI_L("\\")))!=OpStatus::OK ||
				(ret=encoded.Append(end_ptr, 1))!=OpStatus::OK)
			{
                return ret;
			}
        }

        start_ptr = end_ptr+1;
    }

    return string.Set(encoded);
}

OP_STATUS OpMisc::QuoteString(const OpStringC8& source, OpString8& target)
{
	if (source.IsEmpty())
		return target.Set("\"\"");

	if (!target.Reserve(source.Length() * 2 + 2))
		return OpStatus::ERR_NO_MEMORY;

	char* 		target_ptr	= target.CStr();
	const char* source_ptr	= source.CStr();

	// Start quoted string
	*target_ptr++ = '"';

	// Copy characters, escape if necessary
	while (*source_ptr)
	{
		switch(*source_ptr)
		{
			case '"':
			case '\\':
				*target_ptr++ = '\\';
				break;
		}
		*target_ptr++ = *source_ptr++;
	}

	// End quoted string
	*target_ptr++ = '"';
	*target_ptr   = '\0';

	return OpStatus::OK;
}


OP_STATUS OpMisc::UnQuoteString(const OpStringC8& source, OpString8& target)
{
	if (source.IsEmpty() || source[0] != '"')
		return OpStatus::ERR;

	if (!target.Reserve(source.Length()))
		return OpStatus::ERR_NO_MEMORY;

	const char* source_ptr = source.CStr() + 1; // Start after '"'
	char*		target_ptr = target.CStr();

	while (*source_ptr != '"')
	{
		switch (*source_ptr)
		{
			case '\\': // Skip past the escape character
				source_ptr++;
				break;
			case '\0': // String shouldn't end, this is an error
				return OpStatus::ERR;
		}

		// Actual copy
		*target_ptr++ = *source_ptr++;
	}

	// Terminate target
	*target_ptr = '\0';

	return OpStatus::OK;
}


OP_STATUS OpMisc::ConvertFromIMAAAddress(const OpStringC8& imaa_address, OpString16& username, OpString16& domain)
{
	username.Empty();
	domain.Empty();

	OpString tmp_address;
	BOOL is_mailaddress = TRUE;
	OP_STATUS ret = MessageEngine::GetInstance()->GetGlueFactory()->GetBrowserUtils()->ConvertFromIMAAAddress(imaa_address, tmp_address, is_mailaddress);

	if (ret==OpStatus::OK)
		ret = SplitMailAddress(tmp_address, username, domain);

	if (ret==OpStatus::OK && !is_mailaddress && domain.IsEmpty() && username.HasContent())
	{
		ret = domain.Set(username);
		username.Empty();
	}

	return ret;
}

OP_STATUS OpMisc::ConvertToIMAAAddressList(const OpStringC16& addresslist, BOOL is_mailadresses, OpString8& imaa_addresslist, char separator_char, BOOL* failed_imaa)
{
	imaa_addresslist.Empty();
	if (addresslist.IsEmpty())
		return OpStatus::OK;

	OP_STATUS ret;
	OpString temp_address;
	OpString8 temp_imaa_address;
	const uni_char* address_start = addresslist.CStr();
	const uni_char* address_end;
	const uni_char* separator_pos;
	uni_char uni_separator = separator_char;
	BOOL done;
	BOOL first_address = TRUE;
	BOOL in_quotes = FALSE;
	while(address_start && *address_start)
	{
		//Skip whitespace
		do
		{
			done=TRUE;
			while(*address_start==' ') {address_start++; done=FALSE;}
			while (*address_start==uni_separator) {address_start++; done=FALSE;}
		} while (!done);

		//Locate next separator
		if (is_mailadresses)
		{
			separator_pos = address_start;
			uni_char current_char;
			while ((current_char = *separator_pos) != 0)
			{
				if ((current_char=='@' || current_char==uni_separator) && !in_quotes)
				{
					break;
				}
				else if (current_char == '\\' && *(separator_pos+1)!=0) //quote-pair.
				{
					separator_pos++; //Skip '\'. Quoted character will be skipped below
				}
				else if (current_char == '"')
				{
					in_quotes = !in_quotes;
				}

				separator_pos++;
			}

			//separator_pos now points to '\0', '@', or separator
		}

		//domains cannot have DQUOTEs and QuotePairs. Use a more optimized way of finding separator
		separator_pos = uni_strchr(address_start, uni_separator);
		if (!separator_pos)
			separator_pos = address_start+uni_strlen(address_start);

		//end is one character before separator
		address_end = separator_pos-1;

		//Skip whitespace
		while (address_end>address_start && *address_end==' ') address_end--;

		if (address_start<=address_end)
		{
			BOOL local_failed_imaa = FALSE;

			if ((ret=temp_address.Set(address_start, address_end-address_start+1))!=OpStatus::OK ||
				((ret=ConvertToIMAAAddress(temp_address, is_mailadresses, temp_imaa_address, &local_failed_imaa))!=OpStatus::OK && ret!=OpStatus::ERR_PARSING_FAILED))
			{
				return ret;
			}

			if (failed_imaa)
				*failed_imaa |= local_failed_imaa;

			if (local_failed_imaa)
				temp_imaa_address.Empty();

			if (temp_imaa_address.HasContent())
			{
				if (!first_address)
				{
					if ((ret=imaa_addresslist.Append(&separator_char, 1)) != OpStatus::OK)
						return ret;
				}

				if ((ret=imaa_addresslist.Append(temp_imaa_address)) != OpStatus::OK)
					return ret;

				first_address = FALSE;
			}
		}
		address_start = separator_pos;
	}
	return OpStatus::OK;
}

OP_STATUS OpMisc::ConvertToIMAAAddress(const OpStringC16& address, BOOL is_mailaddress, OpString8& imaa_address, BOOL* failed_imaa)
{
	OP_STATUS ret;
	OpString username, domain;
	if ((ret=SplitMailAddress(address, username, domain)) != OpStatus::OK)
		return ret;

	if (!is_mailaddress && domain.IsEmpty())
	{
		if ((ret=domain.Set(username)) != OpStatus::OK)
			return ret;

		username.Empty();
	}

	return ConvertToIMAAAddress(username, domain, imaa_address, failed_imaa);
}

OP_STATUS OpMisc::ConvertToIMAAAddress(const OpStringC16& username, const OpStringC16& domain, OpString8& imaa_address, BOOL* failed_imaa)
{
	imaa_address.Empty();
	if (failed_imaa)
		*failed_imaa = FALSE;

	if (username.IsEmpty() && domain.IsEmpty()) //nothing to do
		return OpStatus::OK;

	//We don't have true IMAA yet. Expect all localparts to be us-ascii
	if (failed_imaa && username.HasContent())
	{
		const uni_char* username_ptr = username.CStr();
		while (username_ptr && *username_ptr)
		{
			if (*username_ptr<32 || *username_ptr>127)
			{
				*failed_imaa = TRUE;
				break;
			}
			username_ptr++;
		}
	}

	OP_STATUS ret;
	RETURN_IF_ERROR(imaa_address.Set(username.CStr()));	//Ignore upper 8 bit, we don't have true IMAA yet

	if (domain.HasContent())
	{
		OpString imaa_domain16;
		if (!imaa_domain16.Reserve(24+(domain.Length())*4)) //This size should be calculated more accurately
			return OpStatus::ERR_NO_MEMORY;

		TRAP(ret, IDNA::ConvertToIDNA_L(domain.CStr(), imaa_domain16.CStr(), imaa_domain16.Capacity()-1, FALSE));
		if (OpStatus::IsError(ret))
		{
			if (ret == OpStatus::ERR)
			{
				OpStatus::Ignore(imaa_address.Set(UNI_L("invalid")));
				return OpStatus::ERR_PARSING_FAILED;
			}
			else
				return ret;
		}

		if (imaa_domain16.HasContent())
		{
			if (imaa_address.HasContent()) //If address already contains a username, separate username and domain with a '@'
			{
				RETURN_IF_ERROR(imaa_address.Append("@"));
			}

			OpString8 imaa_domain8;
			RETURN_IF_ERROR(imaa_domain8.Set(imaa_domain16.CStr()));
			RETURN_IF_ERROR(imaa_address.Append(imaa_domain8));
		}
	}
	return OpStatus::OK;
}

OP_STATUS OpMisc::SplitMailAddress(const OpStringC& complete_address, OpString& username, OpString& domain)
{
	username.Empty();
	domain.Empty();
	if (complete_address.IsEmpty())
		return OpStatus::OK;

	OpString dummy_comment;
	const uni_char* start_username = uni_rfc822_skipCFWS(complete_address.CStr(), dummy_comment);
	if (!start_username || *start_username==0) //Only a comment/folded whitespace in complete_address?
		return OpStatus::OK;
	if (*start_username == '<')
		start_username++;

	const uni_char* end_username = uni_rfc822_strchr(start_username, '@');

	OP_STATUS ret = OpStatus::OK;
	if (!end_username || end_username>start_username)
	{
		ret = username.Set(start_username, end_username ? end_username-start_username : (int)KAll);
	}

	if (ret==OpStatus::OK && end_username && *end_username=='@')
	{
		ret = domain.Set(end_username+1);
	}

	//Remove content that is often wrongly appended at the end of mail-addresses
	if (domain.HasContent())
	{
		int domain_length = domain.Length();
		uni_char postfix;
		while (domain_length>0 &&
			   ((postfix=*(domain.CStr()+domain_length-1))==';' || postfix==':' || postfix==',' || postfix=='>') )
		{
			*(domain.CStr()+(--domain_length)) = 0;
		}
	}

	return ret;
}

OP_STATUS OpMisc::SplitMailAddress(const OpStringC8& complete_address, OpString8& username, OpString8& domain)
{
	username.Empty();
	domain.Empty();
	if (complete_address.IsEmpty())
		return OpStatus::OK;

	const char* start_username = complete_address.CStr();
	while (start_username && (*start_username==' ' || *start_username=='\t')) start_username++; //Not complete CFWS, but good enough
	if (!start_username || *start_username==0) //Only a comment/folded whitespace in complete_address?
		return OpStatus::OK;

	const char* end_username = rfc822_strchr(start_username, '@');

	OP_STATUS ret = OpStatus::OK;
	if (end_username > start_username)
	{
		ret = username.Set(start_username, end_username-start_username);
	}

	if (ret==OpStatus::OK && *end_username=='@')
	{
		ret = domain.Set(end_username+1);
	}

	//Remove content that is often wrongly appended at the end of mail-addresses
	if (domain.HasContent())
	{
		int domain_length = domain.Length();
		char postfix;
		while (domain_length>0 &&
			   ((postfix=*(domain.CStr()+domain_length-1))==';' || postfix==':' || postfix==',') )
		{
			*(domain.CStr()+(--domain_length)) = 0;
		}
	}

	return ret;
}

OP_STATUS OpMisc::IsIPV4Address(BOOL& is_ipv4, const OpStringC& ip_address)
{
	OpString regexp;
	RETURN_IF_ERROR(regexp.Set("^\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}$"));

	is_ipv4 = MessageEngine::GetInstance()->GetGlueFactory()->GetBrowserUtils()->MatchRegExp(regexp, ip_address);
	return OpStatus::OK;
}


OP_STATUS OpMisc::IsIPV6Address(BOOL& is_ipv6, const OpStringC& ip_address)
{
	// An IPV6 address will, at most, contain 7 colons. If it contains less
	// than 7, some zeros must have been truncated using '::'.
	BOOL is_correct_format = FALSE;
	const UINT colon_count = StringUtils::SubstringCount(ip_address, UNI_L(":"));

	if (colon_count > 7)
		is_correct_format = FALSE;
	else if (colon_count == 7)
		is_correct_format = TRUE;
	else if (colon_count < 7)
	{
		if (StringUtils::SubstringCount(ip_address, UNI_L("::")) == 1)
			is_correct_format = TRUE;
	}

	if (is_correct_format)
	{
		OpString ip_copy;
		RETURN_IF_ERROR(ip_copy.Set(ip_address));

		const int address_length = ip_copy.Length();

		// Check the individual subparts to see if they contain valid hex
		// values.
		OpString regexp;
		RETURN_IF_ERROR(regexp.Set("^[a-fA-F0-9]{1,4}$"));

		is_ipv6 = TRUE;

		BrowserUtils* browser_utils = MessageEngine::GetInstance()->GetGlueFactory()->GetBrowserUtils();
		int find_pos = ip_copy.Find(":");
		while ((find_pos != KNotFound) && is_ipv6)
		{
			if ((find_pos + 1) >= address_length)
			{
				is_ipv6 = FALSE; // Not valid.
				break;
			}

			OpString16 subpart;
			RETURN_IF_ERROR(subpart.Set(ip_copy.CStr(), find_pos));

			if (ip_address[find_pos + 1] == ':')
				++find_pos; // Skip '::'.

			if (subpart.HasContent())
				is_ipv6 = browser_utils->MatchRegExp(regexp, subpart);

			ip_copy.Delete(0, find_pos + 1);
			find_pos = ip_copy.Find(":");
		}

		if (ip_copy.HasContent())
			is_ipv6 = browser_utils->MatchRegExp(regexp, ip_copy);
	}
	else
		is_ipv6 = FALSE;

	return OpStatus::OK;
}


OP_STATUS OpMisc::IsPublicIP(BOOL& is_public, const OpStringC& ip_address)
{
	BOOL is_ipv4 = FALSE;
	BOOL is_ipv6 = FALSE;

	RETURN_IF_ERROR(IsIPV4Address(is_ipv4, ip_address));

	if (!is_ipv4)
		RETURN_IF_ERROR(IsIPV6Address(is_ipv6, ip_address));

	if (is_ipv6)
	{
		is_public = TRUE;
		return OpStatus::OK;
	}

	if (!is_ipv4)
		return OpStatus::ERR;

	is_public = TRUE;

	// The list of invalid ip ranges can be found in rfc3330. Just do a check
	// against these.
	if (ip_address.Find("0.") == 0) // 0.0.0.0/8.
		is_public = FALSE;
	else if (ip_address.Find("10.") == 0) // 10.0.0.0/8.
		is_public = FALSE;
	else if (ip_address.Find("127.") == 0) // 127.0.0.0/8.
		is_public = FALSE;
	else if (ip_address.Find("172.1") == 0 ||
		ip_address.Find("172.2") == 0 ||
		ip_address.Find("172.3") == 0) // 172.16.0.0/12.
	{
		const UINT value = StringUtils::NumberFromString(ip_address, 4);
		if (value == 0)
			return OpStatus::ERR; // Conversion failed?
		else if (value >= 16 && value <= 31)
			is_public = FALSE;
	}
	else if (ip_address.Find("192.0.2.") == 0) // 192.0.2.0/24.
		is_public = FALSE;
	else if (ip_address.Find("192.168.") == 0) // 192.168.0.0/16.
		is_public = FALSE;

	return OpStatus::OK;
}


#ifdef NEEDS_RISC_ALIGNMENT
UINT32 OpMisc::UnSerialize32(const UINT8 *buf)
{
    return (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
}

UINT16 OpMisc::UnSerialize16(const UINT8 *buf)
{
    return (buf[0] << 8) | buf[1];
}
#endif // NEEDS_RISC_ALIGNMENT

void OpMisc::LongToChars(unsigned char* dest, long val)
{
	dest[0] = (unsigned char)(val >> 24);
	dest[1] = (unsigned char)(val >> 16);
	dest[2] = (unsigned char)(val >> 8);
	dest[3] = (unsigned char)(val);
}

void OpMisc::WordToChars(unsigned char* dest, long val)
{
	dest[0] = (unsigned char)(val >> 8);
	dest[1] = (unsigned char)(val);
}

long OpMisc::CharsToLong(unsigned char* text)
{
	return (text[0] << 24) | (text[1] << 16) | (text[2] << 8) | text[3];
}

long OpMisc::CharsToWord(unsigned char* text)
{
	return (text[0] << 8) | text[1];
}

BOOL OpMisc::StringConsistsOf(const OpStringC8& string, int legal_types, const OpStringC8& legal_extrachars)
{
    BOOL illegal;
    const char* temp = string.CStr();

    while (*temp)
    {
        illegal=TRUE;
        if ((legal_types&UPPERCASE) && *temp>='A' && *temp<='Z')
            illegal=FALSE;

        if ((illegal && legal_types&LOWERCASE) && *temp>='a' && *temp<='z')
            illegal=FALSE;

        if ((illegal && legal_types&DIGIT) && *temp>='0' && *temp<='9')
            illegal=FALSE;

		if (illegal && (strchr(legal_extrachars.CStr(), *temp)!=NULL))
            illegal=FALSE;

        if (illegal)
            return FALSE;

        temp++;
    }
    return TRUE;
}


OP_STATUS OpMisc::RelativeURItoAbsoluteURI(OpString& absolute_uri,
	const OpStringC& relative_uri, const OpStringC& base_uri)
{
	// First we check if this allready is a relative uri.
	URL url_rel = urlManager->GetURL(relative_uri.CStr());
	if (url_rel.IsEmpty())
		return OpStatus::ERR;

	if (url_rel.GetServerName() != 0)
	{
		// RETURN_IF_ERROR(absolute_uri.Set(url_rel.UniName()));
		RETURN_IF_ERROR(url_rel.GetAttribute(URL::KUniName_With_Fragment_Username, absolute_uri));
	}
	else
	{
		// Combine the base uri with the relative uri.
		URL url_base = urlManager->GetURL(base_uri.CStr());
		if (url_base.IsEmpty())
			return OpStatus::ERR;

		URL url_abs = urlManager->GetURL(url_base, relative_uri.CStr());
		if (url_abs.IsEmpty())
			return OpStatus::ERR;

		RETURN_IF_ERROR(url_abs.GetAttribute(URL::KUniName_Username_Password_Hidden, absolute_uri));
	}

	return OpStatus::OK;
}

#endif //M2_SUPPORT
