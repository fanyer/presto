/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef HAS_SET_HTTP_DATA
#include "modules/upload/upload.h"
#include "modules/formats/hdsplit.h"

#include "modules/util/handy.h"

#include "modules/olddebug/tstdump.h"

#include "modules/url/tools/arrays.h"

#define CONST_KEYWORD_ARRAY(name) PREFIX_CONST_ARRAY(static, KeywordIndex, name, upload)
#define CONST_KEYWORD_ENTRY(x,y) CONST_DOUBLE_ENTRY(keyword, x, index, y)
#define CONST_KEYWORD_END(name) CONST_END(name)


CONST_KEYWORD_ARRAY(Upload_untrusted_headers_HTTP)
	CONST_KEYWORD_ENTRY(NULL, FALSE)
	CONST_KEYWORD_ENTRY("Accept",TRUE)
	CONST_KEYWORD_ENTRY("Accept-Charset",TRUE)
	CONST_KEYWORD_ENTRY("Accept-Encoding",TRUE)
	CONST_KEYWORD_ENTRY("Authorization",TRUE)
	CONST_KEYWORD_ENTRY("Cache-Control",TRUE)
	CONST_KEYWORD_ENTRY("Connection",TRUE)
	CONST_KEYWORD_ENTRY("Content-Length",TRUE)
	CONST_KEYWORD_ENTRY("Content-Transfer-Encoding",TRUE)
	CONST_KEYWORD_ENTRY("Date",TRUE)
	CONST_KEYWORD_ENTRY("Expect",TRUE)
	CONST_KEYWORD_ENTRY("Host",TRUE)
	CONST_KEYWORD_ENTRY("If-Match",TRUE)
	CONST_KEYWORD_ENTRY("If-Modified-Since",TRUE)
	CONST_KEYWORD_ENTRY("If-None-Match",TRUE)
	CONST_KEYWORD_ENTRY("If-Range",TRUE)
	CONST_KEYWORD_ENTRY("If-Unmodified-Since",TRUE)
	CONST_KEYWORD_ENTRY("Keep-Alive",TRUE)
	CONST_KEYWORD_ENTRY("Pragma",TRUE)
	CONST_KEYWORD_ENTRY("Proxy-Authorization",TRUE)
	CONST_KEYWORD_ENTRY("Referer",TRUE)
	CONST_KEYWORD_ENTRY("TE",TRUE)
	CONST_KEYWORD_ENTRY("Trailer",TRUE)
	CONST_KEYWORD_ENTRY("Transfer-Encoding",TRUE)
	CONST_KEYWORD_ENTRY("Upgrade",TRUE)
	CONST_KEYWORD_ENTRY("User-Agent",TRUE)
CONST_KEYWORD_END(Upload_untrusted_headers_HTTP)

CONST_KEYWORD_ARRAY(Upload_untrusted_headers_Bcc)
	CONST_KEYWORD_ENTRY(NULL, FALSE)
	CONST_KEYWORD_ENTRY("Bcc",TRUE)
CONST_KEYWORD_END(Upload_untrusted_headers_Bcc)

CONST_KEYWORD_ARRAY(Upload_untrusted_headers_HTTPContentType)
	CONST_KEYWORD_ENTRY(NULL, FALSE)
	CONST_KEYWORD_ENTRY("Accept",TRUE)
	CONST_KEYWORD_ENTRY("Accept-Charset",TRUE)
	CONST_KEYWORD_ENTRY("Accept-Encoding",TRUE)
	CONST_KEYWORD_ENTRY("Accept-Language",TRUE)
	//CONST_KEYWORD_ENTRY("Authorization",TRUE)
	CONST_KEYWORD_ENTRY("Cache-Control",TRUE)
	CONST_KEYWORD_ENTRY("Connection",TRUE)
	CONST_KEYWORD_ENTRY("Content-Length",TRUE)
	CONST_KEYWORD_ENTRY("Content-Transfer-Encoding",TRUE)
	CONST_KEYWORD_ENTRY("Content-Type",TRUE)
	CONST_KEYWORD_ENTRY("Expect",TRUE)
	CONST_KEYWORD_ENTRY("Host",TRUE)
	CONST_KEYWORD_ENTRY("If-Match",TRUE)
	CONST_KEYWORD_ENTRY("If-Modified-Since",TRUE)
	CONST_KEYWORD_ENTRY("If-None-Match",TRUE)
	CONST_KEYWORD_ENTRY("If-Range",TRUE)
	CONST_KEYWORD_ENTRY("If-Unmodified-Since",TRUE)
	CONST_KEYWORD_ENTRY("Keep-Alive",TRUE)
	CONST_KEYWORD_ENTRY("Pragma",TRUE)
	CONST_KEYWORD_ENTRY("Proxy-Authorization",TRUE)
	CONST_KEYWORD_ENTRY("Referrer",TRUE)
	CONST_KEYWORD_ENTRY("TE",TRUE)
	CONST_KEYWORD_ENTRY("Trailer",TRUE)
	CONST_KEYWORD_ENTRY("Transfer-Encoding",TRUE)
	CONST_KEYWORD_ENTRY("Upgrade",TRUE)
	CONST_KEYWORD_ENTRY("User-Agent",TRUE)
CONST_KEYWORD_END(Upload_untrusted_headers_HTTPContentType)


Header_List::Header_List()
{
}

void Header_List::SetSeparator(Header_Parameter_Separator sep)
{
	Header_Item *item = First();

	while(item)
	{
		item->SetSeparator(sep);
		item = item->Suc();
	}
}

void Header_List::InitL(const char **header_names, Header_Parameter_Separator sep)
{
	if(header_names)
	{
		OpStackAutoPtr<Header_Item> item(NULL);

		while(*header_names)
		{
			item.reset(FindHeader(*header_names,TRUE));
			if(!item.get())
			{
				item.reset(OP_NEW_L(Header_Item, (sep)));
				
				item->InitL(/*OpStringC8*/(*header_names));
				
				item->Into(this);
			}

			item.release();

			header_names++;
		}
	}
}

Header_Item *Header_List::FindHeader(const OpStringC8 &header_ref, BOOL last)
{
	if(header_ref.IsEmpty())
		return NULL;

	if (last)
	{
		for (Header_Item *item = Last(); item; item = item->Pred())
			if(item->GetName().CompareI(header_ref) == 0)
				return item;
	}
	else
	{
		for (Header_Item *item = First(); item; item = item->Suc())
			if(item->GetName().CompareI(header_ref) == 0)
				return item;
	}

	return NULL;
}

Header_Item *Header_List::InsertHeaderL(const OpStringC8 &new_header_name, Header_InsertPoint insert_point, Header_Parameter_Separator sep, const OpStringC8 &header_ref)
{
	OpStackAutoPtr<Header_Item> item(NULL);

	item.reset(OP_NEW_L(Header_Item, (sep)));
	item->InitL(new_header_name);
	InsertHeader(item.get(), insert_point, header_ref);

	return item.release();
}

void Header_List::InsertHeader(Header_Item *new_header, Header_InsertPoint insert_point, const OpStringC8 &header_ref)
{
	Header_Item *insert_item;

	if(new_header == NULL)
		return;

	if(new_header->InList())
		new_header->Out();

	switch(insert_point)
	{
	case HEADER_INSERT_SORT_BEFORE:
	case HEADER_INSERT_BEFORE:
		insert_item = FindHeader(header_ref, FALSE);
		if(insert_item)
		{
			new_header->Precede(insert_item);
			break;
		}
	case HEADER_INSERT_FRONT:
		new_header->IntoStart(this);
		break;
	case HEADER_INSERT_SORT_AFTER:
	case HEADER_INSERT_AFTER:
		insert_item = FindHeader(header_ref, TRUE);
		if(insert_item)
		{
			new_header->Follow(insert_item);
			break;
		}
	case HEADER_INSERT_LAST:
	default:
		new_header->Into(this);
		break;
	}
}

void Header_List::InsertHeaders(Header_List &new_header, Header_InsertPoint insert_point, const OpStringC8 &header_ref, BOOL remove_existing)
{
	Header_Item *insert_item, *insert_item2, *item, *temp, *temp2;

	if (remove_existing)
		for (item = new_header.First(); item; item = item->Suc())
			RemoveHeader(item->GetName());

	item = new_header.First();
	if(!item)
		return;

	switch(insert_point)
	{
	case HEADER_INSERT_SORT_BEFORE:
		while(item)
		{
			temp = item->Suc();

			insert_item2 = FindHeader(item->GetName(), FALSE);
			if(insert_item2)
			{
				item->Out();
				item->Precede(insert_item2);

				item = temp;
				while(item)
				{
					temp2 = item->Suc();

					if(item->GetName().CompareI(insert_item2->GetName()) == 0)
					{
						item->Out();
						item->Precede(insert_item2);

						if(item == temp)
							temp = temp2;
					}
				
					item = temp2;
				}
			}

			item = temp;
		}

		item = new_header.First();

	case HEADER_INSERT_BEFORE:
		insert_item = FindHeader(header_ref, FALSE);
		if(insert_item)
		{
			item->Out();
			item->Precede(insert_item);
			insert_item = item;
			break;
		}
	case HEADER_INSERT_FRONT:
		item->Out();
		item->IntoStart(this);
		insert_item = item;
		break;
	case HEADER_INSERT_SORT_AFTER:
		while(item)
		{
			temp = item->Suc();

			insert_item2 = FindHeader(item->GetName(), TRUE);
			if(insert_item2)
			{
				item->Out();
				item->Follow(insert_item2);

				insert_item2 = item;

				item = temp;
				while(item)
				{
					temp2 = item->Suc();

					if(item->GetName().CompareI(insert_item2->GetName()) == 0)
					{
						item->Out();
						item->Follow(insert_item2);
						insert_item2 = item;

						if(item == temp)
							temp = temp2;
					}
				
					item = temp2;
				}
			}

			item = temp;
		}

		item = new_header.First();
		// fall-through

	case HEADER_INSERT_AFTER:
		insert_item = FindHeader(header_ref, TRUE);
		if(insert_item)
		{
			item->Out();
			item->Follow(insert_item);
			insert_item = item;
			break;
		}
	case HEADER_INSERT_LAST:
	default:
		item->Out();
		item->Into(this);
		insert_item = item;
		break;
	}

	while((item = new_header.First()) != NULL)
	{
		item->Out();
		item->Follow(insert_item);
		insert_item = item;
	}
}

void Header_List::AddParameterL(const OpStringC8 &header_name, const OpStringC8 &p_value)
{
	Header_Item *item;

	item = FindHeader(header_name, TRUE);

	if(item == NULL)
		item = InsertHeaderL(header_name);

	if(p_value.Length())
		item->AddParameterL(p_value);
}

void Header_List::SetSeparatorL(const OpStringC8 &header_name, Header_Parameter_Separator sep)
{
	Header_Item *item;

	item = FindHeader(header_name, TRUE);
	if(item == NULL)
		item = InsertHeaderL(header_name);

	item->SetSeparator(sep);
}

void Header_List::AddParameterL(const OpStringC8 &header_name, const OpStringC8 &p_name, const OpStringC8 &p_value, BOOL quote_value)
{
	Header_Item *item;

	item = FindHeader(header_name, TRUE);

	if(item == NULL)
		item = InsertHeaderL(header_name);

	if(p_value.Length() || p_name.Length())
		item->AddParameterL(p_name, p_value, quote_value);
}

void Header_List::AddParameterL(const OpStringC8 &header_name, Header_Parameter_Base *param)
{
	OpStackAutoPtr<Header_Parameter_Base> param2(param);
	Header_Item *item;

	item = FindHeader(header_name, TRUE);

	if(item == NULL)
		item = InsertHeaderL(header_name);

	item->AddParameter(param2.release());
}

void Header_List::ClearHeader(const OpStringC8 &header_name)
{
	Header_Item *item = FindHeader(header_name, TRUE);

	if(item != NULL)
		item->ClearParameters();
}

void Header_List::RemoveHeader(const OpStringC8 &header_name)
{
	Header_Item *item;
	
	while ((item = FindHeader(header_name, FALSE)) != NULL)
		OP_DELETE(item);
}

void Header_List::ClearAndAddParameterL(const OpStringC8 &header_name, const OpStringC8 &p_value)
{
	Header_Item *item;

	item = FindHeader(header_name, TRUE);

	if(item == NULL)
		item = InsertHeaderL(header_name);

	item->ClearParameters();

	if(p_value.Length())
		item->AddParameterL(p_value);
}

#ifdef UPLOAD_CLEARADD_PARAMETER_SEP
void Header_List::ClearAndSetSeparatorL(const OpStringC8 &header_name, Header_Parameter_Separator sep)
{
	Header_Item *item;

	item = FindHeader(header_name, TRUE);
	if(item == NULL)
		item = InsertHeaderL(header_name);

	item->ClearParameters();

	item->SetSeparator(sep);
}
#endif

#ifdef UPLOAD_CLEARADD_PARAMETER_PAIR
void Header_List::ClearAndAddParameterL(const OpStringC8 &header_name, const OpStringC8 &p_name, const OpStringC8 &p_value, BOOL quote_value)
{
	Header_Item *item;

	item = FindHeader(header_name, TRUE);

	if(item == NULL)
		item = InsertHeaderL(header_name);

	item->ClearParameters();

	if(p_value.Length() || p_name.Length())
		item->AddParameterL(p_name, p_value, quote_value);
}
#endif

#ifdef UPLOAD_CLEARADD_PARAMETER
void Header_List::ClearAndAddParameterL(const OpStringC8 &header_name, Header_Parameter_Base *param)
{
	OpStackAutoPtr<Header_Parameter_Base> param2(param);
	Header_Item *item;

	item = FindHeader(header_name, TRUE);

	if(item == NULL)
		item = InsertHeaderL(header_name);

	item->ClearParameters();

	item->AddParameter(param2.release());
}
#endif

#ifdef URL_UPLOAD_QP_SUPPORT
void Header_List::AddQuotedPrintableParameterL(const OpStringC8 &header_name, const OpStringC8 &p_name, const OpStringC8 &p_value, const OpStringC8 &charset, Header_Encoding encoding)
{
	Header_Item *item;
	OpStackAutoPtr<Header_QuotedPrintable_Parameter> new_param ( OP_NEW_L(Header_QuotedPrintable_Parameter, ()));

	new_param->InitL(p_name, p_value, charset, encoding, TRUE);

	item = FindHeader(header_name, TRUE);

	if(item == NULL)
		item = InsertHeaderL(header_name);

	item->AddParameter(new_param.release());
}

void Header_List::AddQuotedPrintableParameterL(const OpStringC8 &header_name, const OpStringC8 &p_name, const OpStringC16 &p_value, const OpStringC8 &charset, Header_Encoding encoding)
{
	Header_Item *item;
	OpStackAutoPtr<Header_QuotedPrintable_Parameter> new_param ( OP_NEW_L(Header_QuotedPrintable_Parameter, ()));

	new_param->InitL(p_name, p_value, charset, encoding, TRUE);

	item = FindHeader(header_name, TRUE);

	if(item == NULL)
		item = InsertHeaderL(header_name);

	item->AddParameter(new_param.release());
}
#endif

#ifdef URL_UPLOAD_RFC_2231_SUPPORT
void Header_List::AddRFC2231ParameterL(const OpStringC8 &header_name, const OpStringC8 &p_name, const OpStringC8 &p_value, const OpStringC8 &charset)
{
	Header_Item *item;
	OpStackAutoPtr<Header_RFC2231_Parameter> new_param ( OP_NEW_L(Header_RFC2231_Parameter, ()));

	new_param->InitL(p_name, p_value, charset);

	item = FindHeader(header_name, TRUE);

	if(item == NULL)
		item = InsertHeaderL(header_name);

	item->AddParameter(new_param.release());
}

void Header_List::AddRFC2231ParameterL(const OpStringC8 &header_name, const OpStringC8 &p_name, const OpStringC16 &p_value, const OpStringC8 &charset)
{
	Header_Item *item;
	OpStackAutoPtr<Header_RFC2231_Parameter> new_param ( OP_NEW_L(Header_RFC2231_Parameter, ()));

	new_param->InitL(p_name, p_value, charset);

	item = FindHeader(header_name, TRUE);

	if(item == NULL)
		item = InsertHeaderL(header_name);

	item->AddParameter(new_param.release());
}
#endif // URL_UPLOAD_RFC_2231_SUPPORT

#ifdef UPLOAD_HDR_IS_ENABLED
BOOL Header_List::HeaderIsEnabled(const OpStringC8 &header)
{
	Header_Item *item;

	item = FindHeader(header, TRUE);

	return !item || item->IsEnabled(); // default for all is enabled
}
#endif

void Header_List::HeaderSetEnabledL(const OpStringC8 &header, BOOL flag)
{
	Header_Item *item = First();

	BOOL found = FALSE;

	while(item)
	{
		if(item->GetName().CompareI(header) == 0)
		{
			found = TRUE;
			item->SetEnabled(flag);
		}
		item = item->Suc();
	}

	if(!found)
	{
		item = InsertHeaderL(header);
		item->SetEnabled(flag);
	}
}

uint32 Header_List::CalculateLength()
{
	uint32 len=0;
	Header_Item *item = First();

	while(item)
	{
		len += item->CalculateLength();
		item = item->Suc();
	}

	return len;
}

char *Header_List::OutputHeaders(char *target)
{
	Header_Item *item = First();

	*target = '\0'; // Null terminate in case of no output

	while(item)
	{
		target = item->OutputHeader(target);
		item = item->Suc();
	}

	return target;
}

uint32 Header_List::ExtractHeadersL(const unsigned char *src, uint32 len, BOOL use_all, Header_Protection_Policy policy, const KeywordIndex *excluded_headers, int excluded_list_len)
{
	if(src == NULL || len == 0)
		return 0;

	const unsigned char *pos = src;
	unsigned char token;
	uint32 i;
	
	for(i=0;i< len;i++)
	{
		token = *(pos++);
		if(token < 0x20)
		{
			if(token == '\n')
			{
				if(i == len-1 || *pos == '\n')
					break;
				
				if(*pos != '\r')
					continue;
				
				i++;
				pos++;
				if(i == len-1 || *pos == '\n')
					break;
			}
			else if(token != '\t' && token != '\r')
			{
				return 0;
				break;
			}
		}
	}
	
	if(use_all || i < len)
	{
		const KeywordIndex *keywords_list = NULL;
		int keywords_len = 0;
		ANCHORD(HeaderList, untrusted_headers);
		ANCHORD(OpString8, src_str);
		
		src_str.SetL((const char *)src, i+1);
		untrusted_headers.SetValueL(src_str.CStr());
				
		switch(policy)
		{
		case HEADER_REMOVE_HTTP:
			keywords_list =g_Upload_untrusted_headers_HTTP;
			keywords_len = (int)CONST_ARRAY_SIZE(upload, Upload_untrusted_headers_HTTP);
			break;
		case HEADER_REMOVE_HTTP_CONTENT_TYPE:
			keywords_list =g_Upload_untrusted_headers_HTTPContentType;
			keywords_len = (int)CONST_ARRAY_SIZE(upload, Upload_untrusted_headers_HTTPContentType);
			break;
		case HEADER_REMOVE_BCC:
			keywords_list =g_Upload_untrusted_headers_Bcc;
			keywords_len = (int)CONST_ARRAY_SIZE(upload, Upload_untrusted_headers_Bcc);
			break;
		}

		HeaderEntry *next_item = untrusted_headers.First();
		HeaderEntry *item; 
		while(next_item)
		{
			item = next_item;
			next_item = next_item->Suc();
			
			if(item->Name())
			{
				if (policy == HEADER_REMOVE_HTTP && op_strnicmp(item->Name(), "sec-", 4) == 0)
					continue;// Skip, DO NOT add, untrusted header that starts with "sec-", as these are reserved for websockets.
				if(CheckKeywordsIndex(item->Name(), keywords_list, keywords_len) > 0)
					continue; // Skip, DO NOT add, untrusted header
				if(excluded_headers && excluded_list_len &&
					CheckKeywordsIndex(item->Name(), excluded_headers, excluded_list_len) > 0)
					continue; // Skip, DO NOT add, untrusted header
			}
			
			AddParameterL(item->Name(), item->Value());
		}

		if(!use_all)
			i++;
	}

	return i+1;
}

#endif // HTTP_data
