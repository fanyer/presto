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

#include "modules/util/handy.h" //ARRAY_SIZE
#include "modules/formats/keywordman.h"
#include "modules/url/tools/arrays.h"

#define CONST_KEYWORD_ARRAY(name) PREFIX_CONST_ARRAY(static, KeywordIndex, name, formats)
#define CONST_KEYWORD_ENTRY(x,y) CONST_DOUBLE_ENTRY(keyword, x, index, y)
#define CONST_KEYWORD_END(name) CONST_END(name)


CONST_KEYWORD_ARRAY(HTTPHeaders)
	CONST_KEYWORD_ENTRY(NULL, HTTP_Header_Unknown)
	CONST_KEYWORD_ENTRY("Accept-Ranges", HTTP_Header_Accept_Ranges)
	CONST_KEYWORD_ENTRY("Age", HTTP_Header_Age)
#ifdef HTTP_DIGEST_AUTH
	CONST_KEYWORD_ENTRY("Authentication-Info", HTTP_Header_Authentication_Info )
#endif
	CONST_KEYWORD_ENTRY("Boundary", HTTP_Header_Boundary)
	CONST_KEYWORD_ENTRY("Cache-Control", HTTP_Header_Cache_Control )
	CONST_KEYWORD_ENTRY("Connection", HTTP_Header_Connection )
	CONST_KEYWORD_ENTRY("Content-Disposition", HTTP_Header_Content_Disposition )
	CONST_KEYWORD_ENTRY("Content-Encoding", HTTP_Header_Content_Encoding )
	CONST_KEYWORD_ENTRY("Content-Id", HTTP_Header_Content_ID)
	CONST_KEYWORD_ENTRY("Content-Language", HTTP_Header_Content_Language )
	CONST_KEYWORD_ENTRY("Content-Length", HTTP_Header_Content_Length )
	CONST_KEYWORD_ENTRY("Content-Location", HTTP_Header_Content_Location )
	//CONST_KEYWORD_ENTRY("Content-MD5", HTTP_Header_Content_MD5 )
	CONST_KEYWORD_ENTRY("Content-Range", HTTP_Header_Content_Range )
	CONST_KEYWORD_ENTRY("Content-Transfer-Encoding", HTTP_Header_Content_Transfer_Encoding )
	CONST_KEYWORD_ENTRY("Content-Type", HTTP_Header_Content_Type )
	CONST_KEYWORD_ENTRY("Date", HTTP_Header_Date )
	CONST_KEYWORD_ENTRY("Default-Style", HTTP_Header_Default_Style)
	CONST_KEYWORD_ENTRY("ETag", HTTP_Header_ETag )
	CONST_KEYWORD_ENTRY("Expires", HTTP_Header_Expires )
	CONST_KEYWORD_ENTRY("Keep-Alive", HTTP_Header_Keep_Alive )
	CONST_KEYWORD_ENTRY("Last-Modified", HTTP_Header_Last_Modified )
	CONST_KEYWORD_ENTRY("Link", HTTP_Header_Link)
	CONST_KEYWORD_ENTRY("Location", HTTP_Header_Location )
	CONST_KEYWORD_ENTRY("Powered-By", HTTP_Header_Powered_By )
	CONST_KEYWORD_ENTRY("Pragma", HTTP_Header_Pragma )
	CONST_KEYWORD_ENTRY("Proxy-Authenticate", HTTP_Header_Proxy_Authenticate )
#ifdef HTTP_DIGEST_AUTH
	CONST_KEYWORD_ENTRY("Proxy-Authentication-Info", HTTP_Header_Proxy_Authentication_Info )
#endif
	CONST_KEYWORD_ENTRY("Proxy-Connection", HTTP_Header_Proxy_Connection )
#ifdef _SUPPORT_PROXY_NTLM_AUTH_
	CONST_KEYWORD_ENTRY("Proxy-Support", HTTP_Header_Proxy_Support)
#endif
	CONST_KEYWORD_ENTRY("Refresh", HTTP_Header_Refresh )
	CONST_KEYWORD_ENTRY("Retry-After", HTTP_Header_Retry_After )
	CONST_KEYWORD_ENTRY("Server", HTTP_Header_Server )
	CONST_KEYWORD_ENTRY("Set-Cookie", HTTP_Header_Set_Cookie )
	CONST_KEYWORD_ENTRY("Set-Cookie2", HTTP_Header_Set_Cookie2 )
#ifdef LIBSSL_ENABLE_STRICT_TRANSPORT_SECURITY
	CONST_KEYWORD_ENTRY("Strict-Transport-Security", HTTP_Header_Strict_Transport_security )
#endif
	CONST_KEYWORD_ENTRY("Trailer", HTTP_Header_Trailer )
	CONST_KEYWORD_ENTRY("Transfer-Encoding", HTTP_Header_Transfer_Encoding )
	CONST_KEYWORD_ENTRY("Version", HTTP_Header_Version )
	//CONST_KEYWORD_ENTRY("Via", HTTP_Header_Via )
	//CONST_KEYWORD_ENTRY("Warning", HTTP_Header_Warning )
	CONST_KEYWORD_ENTRY("WWW-Authenticate", HTTP_Header_WWW_Authenticate )
	CONST_KEYWORD_ENTRY("X-Frame-Options", HTTP_Header_X_Frame_Options )
	CONST_KEYWORD_ENTRY("X-Powered-By", HTTP_Header_X_Powered_By )
CONST_KEYWORD_END(HTTPHeaders)

#define KEYWORD_TABLE_SIZE_HTTPHeaders CONST_DOUBLE_ARRAY_SIZE(formats, HTTPHeaders)

#ifdef _ENABLE_AUTHENTICATE
CONST_KEYWORD_ARRAY(HTTP_Authentication_Keywords)
	CONST_KEYWORD_ENTRY(NULL, HTTP_Authentication_Unknown)
#ifdef HTTP_DIGEST_AUTH
	CONST_KEYWORD_ENTRY("algorithm", HTTP_Authentication_Algorithm )
	CONST_KEYWORD_ENTRY("auth", HTTP_Authentication_Method_Auth )
	CONST_KEYWORD_ENTRY("auth-int", HTTP_Authentication_Method_Auth_Int )
#endif
	CONST_KEYWORD_ENTRY("Basic", HTTP_Authentication_Method_Basic )
#ifdef HTTP_DIGEST_AUTH
	CONST_KEYWORD_ENTRY("cnonce", HTTP_Authentication_Cnonce )
	CONST_KEYWORD_ENTRY("Digest", HTTP_Authentication_Method_Digest )
	CONST_KEYWORD_ENTRY("domain", HTTP_Authentication_Domain)
	CONST_KEYWORD_ENTRY("nc", HTTP_Authentication_NC )
#endif
#ifdef _SUPPORT_PROXY_NTLM_AUTH_
	CONST_KEYWORD_ENTRY("Negotiate", HTTP_Authentication_Method_Negotiate )
#endif
#ifdef HTTP_DIGEST_AUTH
	CONST_KEYWORD_ENTRY("nextnonce", HTTP_Authentication_Nextnonce )
	CONST_KEYWORD_ENTRY("nonce", HTTP_Authentication_Nonce )
	CONST_KEYWORD_ENTRY("nonce-count", HTTP_Authentication_Nonce_Count )
#endif
#ifdef _SUPPORT_PROXY_NTLM_AUTH_
	CONST_KEYWORD_ENTRY("NTLM", HTTP_Authentication_Method_NTLM )
#endif
#ifdef HTTP_DIGEST_AUTH
	CONST_KEYWORD_ENTRY("opaque", HTTP_Authentication_Opaque )
	CONST_KEYWORD_ENTRY("qop", HTTP_Authentication_Qop )
#endif
	CONST_KEYWORD_ENTRY("realm", HTTP_Authentication_Realm )
#ifdef HTTP_DIGEST_AUTH
	CONST_KEYWORD_ENTRY("rspauth", HTTP_Authentication_Rspauth)
	CONST_KEYWORD_ENTRY("stale", HTTP_Authentication_Stale)
#endif
CONST_KEYWORD_END(HTTP_Authentication_Keywords)

#define KEYWORD_TABLE_SIZE_HTTP_Authentication_Keywords CONST_DOUBLE_ARRAY_SIZE(formats, HTTP_Authentication_Keywords)
#endif // Authenticate

CONST_KEYWORD_ARRAY(HTTP_Cookie_Keywords)
	CONST_KEYWORD_ENTRY(NULL, HTTP_Cookie_Unknown)
	CONST_KEYWORD_ENTRY("Comment", HTTP_Cookie_Comment )
	CONST_KEYWORD_ENTRY("CommentURL", HTTP_Cookie_CommentURL )
	CONST_KEYWORD_ENTRY("Discard", HTTP_Cookie_Discard )
	CONST_KEYWORD_ENTRY("Domain", HTTP_Cookie_Domain )
	CONST_KEYWORD_ENTRY("Expires", HTTP_Cookie_Expires )
	CONST_KEYWORD_ENTRY("Max-Age", HTTP_Cookie_Max_Age )
	CONST_KEYWORD_ENTRY("Path", HTTP_Cookie_Path )
	CONST_KEYWORD_ENTRY("Port", HTTP_Cookie_Port )
	CONST_KEYWORD_ENTRY("Secure", HTTP_Cookie_Secure )
	CONST_KEYWORD_ENTRY("Version",  HTTP_Cookie_Version )
CONST_KEYWORD_END(HTTP_Cookie_Keywords)

#define KEYWORD_TABLE_SIZE_HTTP_Cookie_Keywords CONST_DOUBLE_ARRAY_SIZE(formats, HTTP_Cookie_Keywords)


CONST_KEYWORD_ARRAY(HTTP_General_Keywords)
	CONST_KEYWORD_ENTRY(NULL, HTTP_General_Tag_Unknown)
	CONST_KEYWORD_ENTRY("access-type", HTTP_General_Tag_Access_Type)
	CONST_KEYWORD_ENTRY("attachment", HTTP_General_Tag_Attachment)
	CONST_KEYWORD_ENTRY("base64", HTTP_General_Tag_Base64)
	CONST_KEYWORD_ENTRY("boundary", HTTP_General_Tag_Boundary)
	CONST_KEYWORD_ENTRY("bytes", HTTP_General_Tag_Bytes)
	CONST_KEYWORD_ENTRY("charset", HTTP_General_Tag_Charset)
	CONST_KEYWORD_ENTRY("chunked", HTTP_General_Tag_Chunked)
	CONST_KEYWORD_ENTRY("close", HTTP_General_Tag_Close)
	//CONST_KEYWORD_ENTRY("compress", HTTP_General_Tag_Compress)
	CONST_KEYWORD_ENTRY("deflate", HTTP_General_Tag_Deflate)
	CONST_KEYWORD_ENTRY("delsp", HTTP_General_Tag_Delsp)
	CONST_KEYWORD_ENTRY("directory", HTTP_General_Tag_Directory)
	CONST_KEYWORD_ENTRY("filename", HTTP_General_Tag_Filename)
	CONST_KEYWORD_ENTRY("format", HTTP_General_Tag_Format)
	CONST_KEYWORD_ENTRY("fwd", HTTP_General_Tag_FWD)
	CONST_KEYWORD_ENTRY("gzip", HTTP_General_Tag_Gzip)
	CONST_KEYWORD_ENTRY("identity", HTTP_General_Tag_Identity)
#ifdef LIBSSL_ENABLE_STRICT_TRANSPORT_SECURITY
	CONST_KEYWORD_ENTRY("includeSubDomains", HTTP_General_Include_SubDomains)
#endif
	CONST_KEYWORD_ENTRY("inline", HTTP_General_Tag_Inline)
	CONST_KEYWORD_ENTRY("Keep-Alive", HTTP_General_Tag_Keep_Alive)
	CONST_KEYWORD_ENTRY("max", HTTP_General_Tag_Max)
	CONST_KEYWORD_ENTRY("max-age", HTTP_General_Tag_Max_Age)
#if defined(_SUPPORT_SMIME_) || defined(_SUPPORT_OPENPGP_)
	CONST_KEYWORD_ENTRY("micalg", HTTP_General_Tag_Micalg)
#endif
	CONST_KEYWORD_ENTRY("must-revalidate", HTTP_General_Tag_Must_Revalidate)
	CONST_KEYWORD_ENTRY("name", HTTP_General_Tag_Name)
#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && !defined __OEM_OPERATOR_CACHE_MANAGEMENT
	CONST_KEYWORD_ENTRY("never-flush", HTTP_General_Tag_Never_Flush)
#endif
	CONST_KEYWORD_ENTRY("no-cache", HTTP_General_Tag_No_Cache)
	CONST_KEYWORD_ENTRY("no-store", HTTP_General_Tag_No_Store)
	CONST_KEYWORD_ENTRY("none", HTTP_General_Tag_None)
	CONST_KEYWORD_ENTRY("private", HTTP_General_Tag_Private)
#if defined(_SUPPORT_SMIME_) || defined(_SUPPORT_OPENPGP_)
	CONST_KEYWORD_ENTRY("protocol", HTTP_General_Tag_Protocol)
#endif
	CONST_KEYWORD_ENTRY("site", HTTP_General_Tag_Site)
	CONST_KEYWORD_ENTRY("start", HTTP_General_Tag_Start)
	CONST_KEYWORD_ENTRY("url", HTTP_General_Tag_URL)
	//CONST_KEYWORD_ENTRY("x-compress", HTTP_General_Tag_Compress)
	CONST_KEYWORD_ENTRY("x-deflate", HTTP_General_Tag_Deflate)
	CONST_KEYWORD_ENTRY("x-gzip", HTTP_General_Tag_Gzip)

CONST_KEYWORD_END(HTTP_General_Keywords)

#define KEYWORD_TABLE_SIZE_HTTP_General_Keywords CONST_DOUBLE_ARRAY_SIZE(formats, HTTP_General_Keywords)


#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
CONST_KEYWORD_ARRAY(APC_keywords)
	CONST_KEYWORD_ENTRY(NULL,		APC_Keyword_Type_None)
	CONST_KEYWORD_ENTRY("DIRECT",	APC_Keyword_Type_Direct)
	CONST_KEYWORD_ENTRY("PROXY",	APC_Keyword_Type_Proxy)
#ifdef SOCKS_SUPPORT
	CONST_KEYWORD_ENTRY("SOCKS",	APC_Keyword_Type_Socks)
#endif
CONST_KEYWORD_END(APC_keywords)

#define KEYWORD_TABLE_SIZE_APC_keywords CONST_DOUBLE_ARRAY_SIZE(formats, APC_keywords)
#endif // SUPPORT_AUTO_PROXY_CONFIGURATION	



/** MUST match the sequence of the corresponding enums in enum Prepared_KeywordIndexes */
struct Keyword_Index_Item {
	const KeywordIndex *index;
	unsigned int count;
};

#define DECLARE_KEYWORD_ITEM(item) CONST_DOUBLE_ENTRY(index, g_##item , count, (unsigned int)CONST_ARRAY_SIZE(formats, item))

CONST_DOUBLE_ARRAY(Keyword_Index_Item, Keyword_Index_List, formats)
	CONST_DOUBLE_ENTRY(index,NULL, count, 0) // KeywordIndex_None
	DECLARE_KEYWORD_ITEM(HTTPHeaders) // KeywordIndex_HTTP_MIME
	DECLARE_KEYWORD_ITEM(HTTP_General_Keywords) // KeywordIndex_HTTP_General_Parametrs
#ifdef _ENABLE_AUTHENTICATE
	DECLARE_KEYWORD_ITEM(HTTP_Authentication_Keywords) // KeywordIndex_Authentication
#endif
	DECLARE_KEYWORD_ITEM(HTTP_Cookie_Keywords) // KeywordIndex_Cookies
#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
	DECLARE_KEYWORD_ITEM(APC_keywords)	// KeywordIndex_Automatic_Proxy_Configuration
#endif
CONST_DOUBLE_END(Keyword_Index_List)

#define Keyword_Index_List_Count CONST_ARRAY_SIZE(formats, Keyword_Index_List)

extern "C" {

int list_stricmp(const void *arg1,const void *arg2)
{
	const char *str1 = *((const char **) arg1);
	const char *str2 = *((const char **) arg2);
	if( str1 == str2)
		return 0;
	return op_stricmp(str1, str2);
}

}

KeywordIndexed_Item::KeywordIndexed_Item()
{
	keyword_index = 0;
}

KeywordIndexed_Item::~KeywordIndexed_Item()
{
}

Keyword_Manager::Keyword_Manager()
{
	//keywords1 = NULL;
	keywords2 = NULL;
	keyword_count = 0;
}

Keyword_Manager::Keyword_Manager(Prepared_KeywordIndexes index)
{
	//keywords1 = NULL;
	keywords2 = NULL;
	keyword_count = 0;
	SetKeywordList(index);
}

Keyword_Manager::Keyword_Manager(const Keyword_Manager &old_keys)
: AutoDeleteHead()
{
	//keywords1 = NULL;
	keywords2 = NULL;
	keyword_count = 0;
	SetKeywordList(old_keys);
}

/*
Keyword_Manager::Keyword_Manager(const char* const *keys, unsigned int count)
{
	SetKeywordList(keys, count);
}
*/

Keyword_Manager::Keyword_Manager(const KeywordIndex *keys, unsigned int count)
{
	SetKeywordList(keys, count);
}

/*
void Keyword_Manager::SetKeywordList(const char* const *keys, unsigned int count)
{
	keywords1 = keys;
	keywords2 = NULL;
	keyword_count = count;
	UpdateListIndexes();
}
*/

void Keyword_Manager::SetKeywordList(const KeywordIndex *keys, unsigned int count)
{
	keywords2 = keys;
	//keywords1 = NULL;
	keyword_count = count;
	UpdateListIndexes();
}

void Keyword_Manager::SetKeywordList(const Keyword_Manager &old_keys)
{
	//keywords1 = old_keys.keywords1;
	keywords2 = old_keys.keywords2;
	keyword_count = old_keys.keyword_count;
	UpdateListIndexes();
}

void Keyword_Manager::SetKeywordList(Prepared_KeywordIndexes index)
{
	if(((unsigned) index) >= Keyword_Index_List_Count)
		index = KeywordIndex_None;

	SetKeywordList(g_Keyword_Index_List[index].index, g_Keyword_Index_List[index].count);
}

void Keyword_Manager::UpdateListIndexes()
{
#ifdef _DEBUG
	// Check that the keywordlist is sorted
#ifdef UNUSED_CODE
	if(keywords1 && keyword_count > 1)
	{
		const char *previous = keywords1[0];
		if(previous == NULL)
		{
			// NULL pointers are NOT allowed in the index list!! 
			OP_ASSERT(0);
		}
		else
		{
			for(unsigned int i = 1; i< keyword_count; i++)
			{
				if(keywords1[i] == NULL)
				{
					// NULL pointers are not allowed in the index list!!
					OP_ASSERT(0);
				}
				else
				{
					int cmp = op_stricmp(previous, keywords1[i]);

					if(cmp > 0)
					{
						// !!!!! The list is not correctly sorted !!!!!
						OP_ASSERT(0);
					}
					else if(cmp == 0)
					{
						// !!!!! Duplicated entries !!!!!
						OP_ASSERT(0);
					}

					previous = keywords1[i];
				}
			}
		}
	}
#endif
	if(keywords2 && keyword_count > 2)
	{
		const char *previous = keywords2[1].keyword;
		if(previous == NULL)
		{
			// NULL pointers are NOT allowed in the index list!! 
			OP_ASSERT(0);
		}
		else
		{
			for(unsigned int i = 2; i< keyword_count; i++)
			{
				if(keywords2[i].keyword == NULL)
				{
					// NULL pointers are not allowed in the index list!!
					OP_ASSERT(0);
				}
				else
				{
					int cmp = op_stricmp(previous, keywords2[i].keyword);

					if(cmp > 0)
					{
						// !!!!! The list is not correctly sorted !!!!!
						OP_ASSERT(0);
					}
					else if(cmp == 0)
					{
						// !!!!! Duplicated entries !!!!!
						OP_ASSERT(0);
					}

					previous = keywords2[i].keyword;
				}
			}
		}
	}
#endif

	KeywordIndexed_Item *item = (KeywordIndexed_Item *) First();
	while(item)
	{
		UpdateIndexID(item);
		item = (KeywordIndexed_Item *) item->Suc();
	}
}

void Keyword_Manager::UpdateIndexID(KeywordIndexed_Item *item)
{
	if(item)
		item->SetNameID(GetKeywordID(item->LinkId()));
}


unsigned int Keyword_Manager::GetKeywordID(const OpStringC8 &name) const
{
	if(name.HasContent() && keyword_count)
	{
		if(keywords2)
		{
			return (unsigned int) CheckKeywordsIndex(name.CStr(), keywords2, keyword_count);
		}
		
#ifdef UNUSED_CODE
		if(keywords1)
		{
			const char *tag0 = name.CStr();
			const char* const *tag1;
			
			//int i;
			//for (i = keyword_count, tag1 = keywords;i>0 && *tag1 != tag0; i--, tag1++) {};
			
			//if(i==0)
			tag1 = (const char* const *) op_bsearch(&tag0,
				keywords1, keyword_count,
				sizeof(const char *), list_stricmp);
			
			if(tag1 != NULL && *tag1!= NULL)
				return ((tag1-keywords1) +1);
		}
#endif
	}
	
	return 0;
}

KeywordIndexed_Item *Keyword_Manager::GetItem(const char *tag,
				KeywordIndexed_Item *after,
				Keyword_ResolvePolicy resolve)
{
	if(tag == NULL && after != NULL)
		tag = after->LinkId();
	if(tag == NULL || *tag == '\0')
		return (KeywordIndexed_Item *) (after ? after->Suc() : First());
#ifdef _DEBUG
	BOOL resolved = FALSE;
#endif
	
	KeywordIndexed_Item *current;
	if(resolve == KEYWORD_RESOLVE)
	{
		int id = GetKeywordID(tag);

		if(id)
		{
			current = GetItemByID(id, after);
			if(current)
				return current;

#ifdef _DEBUG
			resolved = TRUE;
#endif
		}
	}
	
	current = (KeywordIndexed_Item *) (after ? after->Suc() : First());
	while(current)
	{
		const char *current_tag = current->LinkId();
		if(current_tag && op_stricmp(current_tag, tag) == 0)
		{
#ifdef _DEBUG
			OP_ASSERT(!resolved);
#endif
			break;
		}
		
		current = (KeywordIndexed_Item *) current->Suc();
	}
	
	return current;
}

KeywordIndexed_Item *Keyword_Manager::GetItemByID(unsigned int tag_id,
				const KeywordIndexed_Item *after) const
{
	KeywordIndexed_Item *current;

	if(!tag_id && after)
		tag_id = after->GetNameID();

	current = (KeywordIndexed_Item *) (after ? after->Suc() : First());

	if(!tag_id)
		return current;

	while(current)
	{
		if(current->GetNameID() == tag_id)
			break;

		current = (KeywordIndexed_Item *) current->Suc();
	}

	return current;
}
