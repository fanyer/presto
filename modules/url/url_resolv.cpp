/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve Pettersen
 */
#include "core/pch.h"

#include "modules/util/str.h"
#include "modules/util/htmlify.h"
#include "modules/formats/uri_escape.h"

#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/util/timecache.h"

#include "modules/encodings/encoders/utf8-encoder.h"
#include "modules/encodings/decoders/utf8-decoder.h"

#include "modules/locale/oplanguagemanager.h"

#include "modules/logdoc/logdoc_util.h"

#include "modules/olddebug/timing.h"
#include "modules/olddebug/tstdump.h"

#include "modules/url/url2.h"

#include "modules/url/prot_sel.h"

#include "modules/url/url_rep.h"
#include "modules/url/url_rel.h"
#include "modules/url/url_man.h"
#include "modules/url/tools/arrays.h"
#include "modules/util/handy.h"
#include "modules/about/opillegalhostpage.h"
#include "modules/pi/OpSystemInfo.h"

#ifdef GADGET_SUPPORT
#include "modules/gadgets/OpGadget.h"
#include "modules/gadgets/OpGadgetManager.h"
#endif /* GADGET_SUPPORT */

#ifdef NEED_URL_HANDLE_UNRECOGNIZED_SCHEME
extern void HandleUnrecognisedURLTypeL(const OpStringC& aUrlName);
#endif

#include "modules/url/url_types.h"

#ifdef NEED_URL_REMAP_RESOLVING
#include "modules/url/remap.h"
#endif

#ifdef APPLICATION_CACHE_SUPPORT
#include "modules/applicationcache/application_cache_manager.h"
#endif // APPLICATION_CACHE_SUPPORT

#ifdef DOM_JIL_API_SUPPORT
#include "modules/device_api/jil/JILFSMgr.h"
#endif // DOM_JIL_API_SUPPORT

OP_STATUS URL_Manager::CheckTempbuffers(unsigned checksize)
{
	if(temp_buffer_len < checksize)
	{
		unsigned int t_temp_buffer_len = temp_buffer_len;
		uni_char *ut_temp_buffer1 = uni_temp_buffer1;
		uni_char *ut_temp_buffer2 = uni_temp_buffer2;
		uni_char *ut_temp_buffer3 = uni_temp_buffer3;
		
		// Make the buffer size a multiple of 256.
		temp_buffer_len = (checksize + 255 ) & ~255;
		uni_temp_buffer1 = OP_NEWA(uni_char, temp_buffer_len+1);
		uni_temp_buffer2 = OP_NEWA(uni_char, temp_buffer_len+1);
		uni_temp_buffer3 = OP_NEWA(uni_char, temp_buffer_len+1);
		temp_buffer1 = (char *) uni_temp_buffer3;
		temp_buffer2 = (char *) uni_temp_buffer2;
		
		if(uni_temp_buffer1 == NULL || uni_temp_buffer2 == NULL || uni_temp_buffer3 == NULL
			)
		{
			OP_DELETEA(uni_temp_buffer1);
			OP_DELETEA(uni_temp_buffer2);
			OP_DELETEA(uni_temp_buffer3);
			
			temp_buffer_len = t_temp_buffer_len;
			uni_temp_buffer1 = ut_temp_buffer1;
			uni_temp_buffer2 = ut_temp_buffer2;
			uni_temp_buffer3 = ut_temp_buffer3;
			temp_buffer1 = (char *) uni_temp_buffer3;
			temp_buffer2 = (char *) uni_temp_buffer2;
			
			g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
			
			return OpStatus::ERR_NO_MEMORY;
		}

		op_memcpy(uni_temp_buffer1, ut_temp_buffer1, UNICODE_SIZE(t_temp_buffer_len));
		op_memcpy(uni_temp_buffer2, ut_temp_buffer2, UNICODE_SIZE(t_temp_buffer_len));
		op_memcpy(uni_temp_buffer3, ut_temp_buffer3, UNICODE_SIZE(t_temp_buffer_len));
		
		OP_DELETEA(ut_temp_buffer1);
		OP_DELETEA(ut_temp_buffer2);
		OP_DELETEA(ut_temp_buffer3);

		if (temp_buffer_len > TEMPBUFFER_SHRINK_LIMIT)
			urlManager->PostShrinkTempBufferMessage();
	}

	if(uni_temp_buffer1 == NULL || uni_temp_buffer2 == NULL || uni_temp_buffer3 == NULL)
		return OpStatus::ERR_NO_MEMORY;

	return OpStatus::OK;
}

void URL_Manager::ShrinkTempbuffers()
{
	if(temp_buffer_len > TEMPBUFFER_SHRINK_LIMIT)
	{
		OP_DELETEA(uni_temp_buffer1);
		OP_DELETEA(uni_temp_buffer2);
		OP_DELETEA(uni_temp_buffer3);

		temp_buffer_len = URL_INITIAL_TEMPBUFFER_SIZE;
		uni_temp_buffer1 = OP_NEWA(uni_char, temp_buffer_len+1);
		uni_temp_buffer2 = OP_NEWA(uni_char, temp_buffer_len+1);
		uni_temp_buffer3 = OP_NEWA(uni_char, temp_buffer_len+1);
		if(uni_temp_buffer1 == NULL || uni_temp_buffer2 == NULL || uni_temp_buffer3 == NULL)
		{
			OP_DELETEA(uni_temp_buffer1);
			OP_DELETEA(uni_temp_buffer2);
			OP_DELETEA(uni_temp_buffer3);

			temp_buffer_len = 0;
			uni_temp_buffer1 = NULL;
			uni_temp_buffer2 = NULL;
			uni_temp_buffer3 = NULL;
		}
		temp_buffer1 = (char *) uni_temp_buffer3;
		temp_buffer2 = (char *) uni_temp_buffer2;
		OP_ASSERT(temp_buffer_len <= TEMPBUFFER_SHRINK_LIMIT);
	}
}

void URL_Manager::PostShrinkTempBufferMessage()
{
	if (!posted_temp_buffer_shrink_msg)
		posted_temp_buffer_shrink_msg = g_main_message_handler->PostMessage(MSG_URL_SHRINK_TEMPORARY_BUFFERS, 0, 0);
}

URL URL_Manager::GetNewOperaURL()
{
	uni_char *buf = (uni_char *) g_memory_manager->GetTempBufUni();
	uni_snprintf(buf,64, UNI_L("opera:%u"), opera_url_number++);
	buf[63] = 0;
	return GetURL(buf);
}

#ifdef URL_ACTIVATE_GET_URL_BY_DATAFILE_RECORD
URL	URL_Manager::GetURL(DataFile_Record *rec, FileName_Store &filenames, BOOL unique, URL_CONTEXT_ID ctx_id)
{
	URL_Rep* new_rep = NULL;
	
#ifndef SEARCH_ENGINE_CACHE
	TRAPD(op_err, URL_Rep::CreateL(&new_rep, rec, filenames, OPFILE_CACHE_FOLDER, ctx_id));
#else
	TRAPD(op_err, URL_Rep::CreateL(&new_rep, rec, OPFILE_CACHE_FOLDER, ctx_id));
#endif
	OpStatus::Ignore(op_err);

	if(new_rep)
	{
		if(unique)
		{
			new_rep->SetAttribute(URL::KUnique,TRUE);
			new_rep->DecRef();
		}
		else if(!new_rep->InList())
			AddURL_Rep(new_rep);
	}
	
	return URL(new_rep, (char *) NULL);
}
#endif // URL_ACTIVATE_GET_URL_BY_DATAFILE_RECORD

extern BOOL RemapUrlL(const OpStringC&,OpString&);

URL URL_Manager::LocalGetURL(const URL* prnt_url, const OpStringC &url, const OpStringC &rel, BOOL unique, URL_CONTEXT_ID a_ctx_id)
{
	OP_MEMORY_VAR URL_CONTEXT_ID ctx_id = a_ctx_id;
#ifdef _DEBUG_URLMAN_CHECK
	Check();
#endif

	if(url.CStr() == NULL)
		return URL();

	URL base2;
	if (prnt_url)
	{
		base2 = prnt_url->GetAttribute(URL::KBaseAliasURL);
		if(base2.IsValid())
			prnt_url = &base2;
	}

	if(prnt_url && prnt_url->IsValid() && prnt_url->GetContextId())
		ctx_id = prnt_url->GetContextId();

	
	
	OP_STATUS op_err = OpStatus::OK;
	OpString temp_url;

	if(prnt_url && prnt_url->IsValid() && url.IsEmpty())
	{
		// Optimization
		// if the relative URL is empty, just use the Base URL as a reference.
		do{
			URLType purl_type = (URLType) prnt_url->GetAttribute(URL::KType);

			if (purl_type == URL_JAVASCRIPT)
				break; // Javascript URLs cannot have relative URLs

			if(unique)
			{
				break; // Full processing for unique URLs, usi
			}
			
			OpString8 rel2;
			OpStatus::Ignore(rel2.SetUTF8FromUTF16(rel)); // noting really major breaks if this fails.

			return URL(prnt_url->GetRep(), rel2.CStr());
		}while(0);
	}

	URL_Name_Components_st resolved_name;

	do{
		op_err = GetResolvedNameRep(resolved_name, (prnt_url ? prnt_url->rep : 0), url.CStr());
		
		if(OpStatus::IsError(op_err) || !resolved_name.full_url || !(*resolved_name.full_url))
			break;
		
#ifdef NEED_URL_REMAP_RESOLVING
		OpString url_remap;
		OpString temp_url;
		if(OpStatus::IsSuccess(temp_url.Set(resolved_name.full_url)))
		{
			op_err=RemapUrl(temp_url,url_remap
#ifdef NEED_URL_REMAP_RESOLVING_CONTEXT_ID
				, ctx_id
#endif 
				);
			if(OpStatus::IsError(op_err))
				break;
			if (url_remap.CStr())
			{
				op_err = GetResolvedNameRep(resolved_name, ((prnt_url) ? prnt_url->rep : 0), url_remap.CStr());
			}
		}
#endif // NEED_URL_REMAP_RESOLVING
	}while(0);
	
	if(!resolved_name.full_url || !(*resolved_name.full_url))
	{
		if(!OpStatus::IsMemoryError(op_err) && url.HasContent())
		{
			const uni_char *temp = url.CStr();

			while(*temp && uni_isspace(*temp))
				temp++;

			if(*temp)
			{
				return GenerateInvalidHostPageUrl(temp, ctx_id
#ifdef ERROR_PAGE_SUPPORT
				, ((op_err == OpStatus::ERR_OUT_OF_RANGE) ? OpIllegalHostPage::KIllegalPort : OpIllegalHostPage::KIllegalCharacter)
#endif // ERROR_PAGE_SUPPORT
					);
			}
		}
		return URL();
	}

#ifdef APPLICATION_CACHE_SUPPORT
	ApplicationCacheManager *app_man;
	if (!unique && ctx_id == 0 && (app_man = g_application_cache_manager))
	{
		URL_CONTEXT_ID application_cache_url_context = app_man->GetMostAppropriateCache(url.CStr(), prnt_url ? *prnt_url : URL());
		if (application_cache_url_context > 0)
		{
			ctx_id = application_cache_url_context;
		}
	}
#endif // APPLICATION_CACHE_SUPPORT

	OpString8 rel2;
	OpStatus::Ignore(rel2.SetUTF8FromUTF16(rel)); // noting really major breaks if this fails.

	URL_Rep *url_rep = NULL;
	if (unique)
	{
		OP_STATUS op_err;

		op_err = URL_Rep::Create(&url_rep, resolved_name, ctx_id);

		RAISE_AND_RETURN_VALUE_IF_ERROR(op_err, URL());//FIXME:OOM7 Can't return error value...

		url_rep->SetAttribute(URL::KUnique,TRUE);
		url_rep->DecRef();
	}
	else
	{
		url_rep = GetResolvedURL(resolved_name, ctx_id);

#ifdef _MIME_SUPPORT_
		if(url_rep && url_rep->GetAttribute(URL::KIsAlias))
			return URL(url_rep->GetAttribute(URL::KAliasURL), rel.CStr());
#endif
	}

#ifdef URL_FILE_SYMLINKS
	OpString sym_link;
	OpString path;
	path.Set(resolved_name.path);
	if (url_rep && !path.IsEmpty() && prnt_url && prnt_url->Type() == URL_WIDGET &&
		resolved_name.url_type == URL_FILE &&
		OpStatus::IsSuccess(g_DAPI_jil_fs_mgr->JILToSystemPath(path.CStr(), sym_link)))
	{
#ifdef SYS_CAP_FILESYSTEM_HAS_DRIVES
		// converted paths are always absolute, make sure they start with a /
		if (sym_link[0] != '/')
			RETURN_VALUE_IF_ERROR(sym_link.Insert(0, UNI_L("/")), URL());
#endif // SYS_CAP_FILESYSTEM_HAS_DRIVES
		
		if(!g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseUTF8Urls))
		{
			UriEscape::Escape(uni_temp_buffer2, sym_link.CStr(), UriEscape::Range_80_ff);
			RETURN_VALUE_IF_ERROR(sym_link.Set(uni_temp_buffer2), URL());
		}

		unsigned escaped_length = UriEscape::GetEscapedLength(sym_link.CStr(), UriEscape::StandardUnsafe);
		OpString sym_link_escaped;
		RETURN_VALUE_IF_NULL(sym_link_escaped.Reserve(escaped_length), URL());
		UriEscape::Escape(sym_link_escaped.DataPtr(), sym_link.CStr(), UriEscape::StandardUnsafe);

		url_rep->SetAttribute(URL::KUniPath_SymlinkTarget, sym_link_escaped);
	}
#endif // URL_FILE_SYMLINKS

	return URL(url_rep, rel2.CStr());

#ifdef _DEBUG_URLMAN_CHECK
	Check();
#endif
}

URL URL_Manager::GetURL(const OpStringC8 &url, URL_CONTEXT_ID ctx_id)
{
	OpString url1;

	RETURN_VALUE_IF_ERROR(url1.SetFromUTF8(url), URL());//FIXME:OOM7 Can't return error value...

	return  GetURL(url1, ctx_id);
}

URL URL_Manager::GetURL(const URL& prnt_url, const OpStringC8 &url, BOOL unique, URL_CONTEXT_ID ctx_id)
{
	OpString url1;

	RETURN_VALUE_IF_ERROR(url1.SetFromUTF8(url), URL());//FIXME:OOM7 Can't return error value...

	return  GetURL(prnt_url, url1, unique, ctx_id);
}

URL	URL_Manager::GetURL(const OpStringC8 &url, const OpStringC8 &rel, BOOL unique, URL_CONTEXT_ID ctx_id)
{
	OpString url1, rel1;

	RETURN_VALUE_IF_ERROR(url1.SetFromUTF8(url), URL());//FIXME:OOM7 Can't return error value...
	RETURN_VALUE_IF_ERROR(rel1.SetFromUTF8(rel), URL());//FIXME:OOM7 Can't return error value...

	return GetURL(url1, rel1, unique, ctx_id);
}

URL	URL_Manager::GetURL(const URL& prnt_url, const OpStringC8 &url, const OpStringC8 &rel, BOOL unique, URL_CONTEXT_ID ctx_id)
{
	OpString url1, rel1;

	RETURN_VALUE_IF_ERROR(url1.SetFromUTF8(url), URL());//FIXME:OOM7 Can't return error value...
	RETURN_VALUE_IF_ERROR(rel1.SetFromUTF8(rel), URL());//FIXME:OOM7 Can't return error value...

	return GetURL(prnt_url, url1, rel1, unique, ctx_id);
}

URL	URL_Manager::GetURL(const OpStringC &url, URL_CONTEXT_ID ctx_id) 
{
	URL empty;

	return GetURL(empty, url, FALSE, ctx_id);
}

URL	URL_Manager::GetURL(const URL& prnt_url, const OpStringC &url, BOOL unique, URL_CONTEXT_ID ctx_id)
{
	if(url.CStr() == NULL)
		return URL();

	OpString temp_url;

	int rel_url_pos =  url.CompareI(UNI_L("javascript:"), 11) == 0 ? KNotFound : url.FindFirstOf('#');
	if (rel_url_pos != KNotFound)
	{
		RETURN_VALUE_IF_ERROR(temp_url.Set(url.CStr(), rel_url_pos),URL());//FIXME:OOM7 Can't return error value...
		rel_url_pos ++;
	}

	OpStringC temp_url2(temp_url.CStr() != NULL ? temp_url : url);
	OpStringC rel_url(rel_url_pos == KNotFound ? (const uni_char *) NULL : url.CStr() + rel_url_pos);

	return  LocalGetURL(&prnt_url, temp_url2, rel_url, unique, ctx_id);
}

#ifndef BREW // crashes arm compiler. why this at all? (rg)
#ifdef __cplusplus
extern "C" {
#endif
#endif // BREW

int protocol_stricmp(const void *arg1, const void *arg2)
{ // No LogFunc
	protocol_selentry *key = ((protocol_selentry *) arg1);
	protocol_selentry *entry = ((protocol_selentry *) arg2);
	return op_stricmp(key->protocolname, entry->protocolname);
}

int protocol_strcmp(const void *arg1, const void *arg2)
{
	protocol_selentry *key = ((protocol_selentry *) arg1);
	protocol_selentry *entry = ((protocol_selentry *) arg2);
	return op_strcmp(key->protocolname, entry->protocolname);
}

#ifndef BREW
#ifdef __cplusplus
}
#endif
#endif // BREW

class ProtocolList : public Link
{
private:
	OpString8 protname;
	OpString uni_protname;
public:
	protocol_selentry item;

public:
	ProtocolList(){
		item.uni_protocolname = NULL; 
		item.protocolname = NULL; 
		item.real_urltype = item.used_urltype = URL_NULL_TYPE; 
		item.have_servername = FALSE;
		item.default_port = 0;
#ifdef EXTERNAL_PROTOCOL_SCHEME_SUPPORT
		item.is_external = FALSE;
#endif
	}
	virtual ~ProtocolList(){
		if(InList()) 
			Out(); 
	}
	OP_STATUS Construct(const uni_char *name, URLType typ){
		if(name == NULL)
			return OpStatus::ERR_NULL_POINTER;
		RETURN_IF_ERROR(protname.Set(name));
		RETURN_IF_ERROR(uni_protname.Set(name));

		item.uni_protocolname = uni_protname.CStr();
		item.protocolname = protname.CStr();
		item.real_urltype = item.used_urltype = typ; 
#ifdef URL_USE_SCHEME_ALIAS
		item.alias_scheme = NULL;
#endif
		return OpStatus::OK;
	}

	const protocol_selentry *Match(const uni_char* str) const{
		return (uni_protname.Compare(str) == 0 ? &item : NULL);
	}
	ProtocolList *Suc(){return (ProtocolList *) Link::Suc();}
	ProtocolList *Pred(){return (ProtocolList *) Link::Pred();}
};


const protocol_selentry* URL_Manager::LookUpUrlScheme(const char* str, BOOL lowercase)
{
	protocol_selentry searched;
	searched.protocolname = str;
	int (*compare)(const void *, const void *);

	if(lowercase)
		compare = protocol_strcmp;
	else
		compare = protocol_stricmp;

	const protocol_selentry  *entry= (const protocol_selentry *) op_bsearch(&searched, g_URL_typenames+1,
			CONST_ARRAY_SIZE(url, URL_typenames) -1 , sizeof(protocol_selentry),
			compare);

#ifdef URL_USE_SCHEME_ALIAS
	if(entry && entry->alias_scheme)
		entry = LookUpUrlScheme(entry->alias_scheme, TRUE);
#endif

	if(entry == NULL)
	{
		OpString temp_str;

		temp_str.Set(str);

		if(temp_str.HasContent())
			entry = urlManager->GetUrlScheme(temp_str, FALSE);
	}

	return entry;
}

const protocol_selentry* URL_Manager::LookUpUrlScheme(URLType type)
{
	for (size_t i = CONST_ARRAY_SIZE(url, URL_typenames); i--;)
	{
		if (g_URL_typenames[i].real_urltype == type)
			return &g_URL_typenames[i];
	}

	ProtocolList *scheme = (ProtocolList *) urlManager->unknown_url_schemes.First();

	while(scheme)
	{
		if(scheme->item.real_urltype == type)
			return &scheme->item;
		scheme = scheme->Suc();
	}

	return NULL;
}

URLType URL_Manager::MapUrlType(const char* str, BOOL lowercase)
{
	const protocol_selentry  *entry= LookUpUrlScheme(str, lowercase);

	return (entry ? entry->real_urltype : URL_UNKNOWN);
}

const char* URL_Manager::MapUrlType(URLType type)
{
	const protocol_selentry *elm = LookUpUrlScheme(type);

	return (elm ? elm->protocolname : NULL);
}

int protocol_uni_strcmp(const void *arg1, const void *arg2)
{
	protocol_selentry *key = ((protocol_selentry *) arg1);
	protocol_selentry *entry = ((protocol_selentry *) arg2);
	return uni_strcmp(key->uni_protocolname, entry->uni_protocolname);
}

URLType URL_Manager::MapUrlType(const uni_char* str)
{
	protocol_selentry searched;
	searched.uni_protocolname = str;
	const protocol_selentry  *entry= (const protocol_selentry *) op_bsearch(&searched, g_URL_typenames+1,
			CONST_ARRAY_SIZE(url, URL_typenames) -1 , sizeof(protocol_selentry),
			protocol_uni_strcmp);

#ifdef URL_USE_SCHEME_ALIAS
	if(entry && entry->alias_scheme)
		entry = LookUpUrlScheme(entry->alias_scheme, TRUE);
#endif

	if(entry == NULL)
	{
		entry = urlManager->GetUrlScheme(str, FALSE);
	}

	return (entry ? entry->real_urltype : URL_UNKNOWN);
}

const protocol_selentry *URL_Manager::GetUrlScheme(const OpStringC& str, BOOL create
#ifdef EXTERNAL_PROTOCOL_SCHEME_SUPPORT
													, BOOL external
#endif
#if defined URL_ENABLE_REGISTER_SCHEME || defined EXTERNAL_PROTOCOL_SCHEME_SUPPORT
													, BOOL have_servername
#endif
#ifdef URL_ENABLE_REGISTER_SCHEME
													, URLType real_type
#endif
												   )
{
	protocol_selentry searched;
	searched.uni_protocolname = str.CStr();
	const protocol_selentry  *entry= (const protocol_selentry *) op_bsearch(&searched, g_URL_typenames+1,
			CONST_ARRAY_SIZE(url, URL_typenames) -1 , sizeof(protocol_selentry),
			protocol_uni_strcmp);

#ifdef URL_USE_SCHEME_ALIAS
	if(entry && entry->alias_scheme)
		entry = LookUpUrlScheme(entry->alias_scheme, TRUE);
#endif

	if(entry == NULL)
	{
		ProtocolList *scheme = (ProtocolList *) unknown_url_schemes.First();

		while(scheme)
		{
			entry = scheme->Match(str.CStr());
			if(entry)
				break;
			scheme = scheme->Suc();
		}
		
		if(scheme)
		{
			scheme->Out();
			scheme->IntoStart(&unknown_url_schemes);
#ifdef EXTERNAL_PROTOCOL_SCHEME_SUPPORT
			if(create && external)
				scheme->item.is_external = TRUE;
#endif
		}
		else if(create)
		{
			scheme = OP_NEW(ProtocolList, ());
			
			if(scheme)
			{
				last_unknown_scheme = (URLType) (last_unknown_scheme +1);

				if(OpStatus::IsError(scheme->Construct(str, last_unknown_scheme)))
				{
					OP_DELETE(scheme);
				}
				else
				{
#ifdef EXTERNAL_PROTOCOL_SCHEME_SUPPORT
					scheme->item.is_external = external;
#endif
#if defined URL_ENABLE_REGISTER_SCHEME || defined EXTERNAL_PROTOCOL_SCHEME_SUPPORT
					scheme->item.have_servername = have_servername;
#endif
#ifdef URL_ENABLE_REGISTER_SCHEME
					if(real_type != URL_NULL_TYPE)
						scheme->item.real_urltype = real_type;
#endif

					scheme->IntoStart(&unknown_url_schemes);
					entry = &scheme->item;
				}
			}
		}
	}

	return entry;
}

const protocol_selentry *URL_Manager::CheckAbsURL(uni_char* this_name)
{
	const protocol_selentry *scheme = NULL;

	if (this_name)
	{
		uni_char *temp_buf = uni_temp_buffer3; // this_name is always uni_1
		uni_char *target = temp_buf;

		uni_char *tmp = this_name;
		uni_char temp_char;

		while((temp_char = *tmp) != '\0')
		{
			if(temp_char < 'A')
			{
				if(temp_char < '0')
				{
					if(temp_char == '+' || temp_char == '-' || temp_char == '.')
						*(target++) = temp_char;
					else
						break;
				}
				else if(temp_char <= '9')
					*(target++) = temp_char;
				else
					break;
			}
			else if(temp_char >= 'a')
			{
				if(temp_char <= 'z')
					*(target++) = temp_char;
				else
					break;
			}
			else if(temp_char <= 'Z')
				*(target++) = op_tolower(temp_char); // Won't happen often;
			else
				break;


			/*
			if( (temp_char >='a' && temp_char <= 'z') ||
				(temp_char >= '0' && temp_char <= '9') ||
				temp_char == '+' || temp_char == '-' || temp_char == '.')
				*(target++) = temp_char;
			else if (temp_char >='A' && temp_char <= 'Z')
				*(target++) = op_tolower(temp_char); // Won't happen often;
			else 
				break; // non-token character -> quit
				*/
			tmp ++;
		}

		if (temp_char == ':' && target > temp_buf)
		{
			*target = 0;

			scheme = GetUrlScheme(temp_buf);

			if(scheme == NULL)
				scheme = &g_URL_typenames[0]; // Unknown but legal scheme, which is different from a NULL scheme

			switch (scheme->used_urltype)
			{
			case URL_HTTP:
			case URL_HTTPS:
			case URL_FTP:
			case URL_WEBSOCKET:
			case URL_WEBSOCKET_SECURE:
				if (!(tmp[1] == '/' && tmp[2] == '/'))
					return NULL;

			// These URL types are a bit more relaxed; allow for e.g. news:///xxx

#if 0
			case URL_NEWS:
			case URL_SNEWS:
				if (tmp[1] == '/' &&
					tmp[2] == '/' &&
					(tmp[3] == '/' || !tmp[3]))
				return NULL;
#endif // 0
			default: break; // other URL types aren't fussed about //es.
			}
        }
    }

    return scheme;
}

OP_STATUS URL_Manager::GetResolvedNameRep(URL_Name_Components_st &resolved_name, URL_Rep* prnt_url, const uni_char* t_name)
{
//	OP_ASSERT(t_name != NULL);
	if (t_name == NULL)
		t_name = UNI_L("");

	resolved_name.Clear();

	uni_char* OP_MEMORY_VAR this_name = (uni_char*) t_name;

	// Strips leading whitespace
	{
		uni_char tmp_char;

		while((tmp_char = *(this_name)) != 0)
		{
			if(!uni_isspace(tmp_char))
				break;
			this_name ++;
		}
	}

	int utf8_escapes = 0;

	{
		// calculate worstcase number of UTF-8 escapes in this URL (assumes all UTF-8 encodings are 3 bytes -> 9 characters escaped -> extra bytes is 8
		// also inlcudes escapes for a 
		const uni_char *tmp = this_name;
		uni_char tmp_char;

		while((tmp_char = *(tmp++)) != 0)
		{
			if(tmp_char >= 0x80)
				utf8_escapes += 8; // extra bytes needed for %XX escapes  for 3 character UTF-8 sequence.
			//else if (EscapeChar((unsigned char) tmp_char)) // Check this later
			//	utf8_escapes += 2;
		}
	}

	unsigned int prnt_len = (prnt_url ? prnt_url->GetAttribute(URL::KPath).Length() + prnt_url->GetAttribute(URL::KInternal_AuthorityLength) : 0);
	RETURN_IF_ERROR(CheckTempbuffers(prnt_len + uni_strlen(this_name)+ utf8_escapes +256));

	uni_strcpy(uni_temp_buffer1, this_name);
	this_name = uni_temp_buffer1;
	
	// Strips trailing whitespace // cannot use strip()
	{
		uni_char *tmp = this_name; // read pos
		uni_char *tmp1 = this_name; // write pos
		uni_char *tmp2 = this_name; // last non-whitespace pos
		uni_char tmp_char;

		while((tmp_char = *(tmp++)) != 0)
		{
			// remove CR, LF, and TAB throughout.
			if(tmp_char != '\n' && tmp_char != '\r' && tmp_char != '\t')
				*(tmp1++) = tmp_char;

			if((tmp_char > 0x20 && tmp_char < 0x80) || !uni_isspace(tmp_char))
			{
				tmp2 = tmp1; // the character after tmp_char
			}
		}

		*tmp2 = 0;
	}

	uni_char *username = NULL;
	uni_char *password = NULL;
	uni_char *resolved_path = NULL;
	uni_char *resolved_query = NULL;

	URLType purl_type = (prnt_url ? (URLType) prnt_url->GetAttribute(URL::KType) : URL_NULL_TYPE);
	URLType url_type;

	BOOL non_relative_base = purl_type == URL_JAVASCRIPT ||
		purl_type == URL_OPERA || purl_type == URL_DATA;

	if(purl_type != URL_NULL_TYPE && this_name[0] == '/' && this_name[1] == '/' && !non_relative_base)
		// Handles gopher style URLs
		resolved_name.scheme_spec = (const protocol_selentry *) prnt_url->GetAttribute(URL::KProtocolScheme, NULL);
	else
		resolved_name.scheme_spec = CheckAbsURL(this_name);

	resolved_name.url_type = url_type = (resolved_name.scheme_spec ? resolved_name.scheme_spec->real_urltype : URL_NULL_TYPE) ;

    if (url_type != URL_NULL_TYPE || non_relative_base)
    {
		if(url_type != URL_NULL_TYPE && resolved_name.scheme_spec->have_servername)
		{
			// Find server name;
			uni_char *tmp;
			
			if(this_name[0] == '/' && this_name[1] == '/')
				tmp = this_name + 2; // Handles gopher style URLs
			else
			{
				tmp = uni_strstr(this_name,UNI_L("://"));

				if(tmp)
					tmp += 3;
			}
			if (!tmp && url_type == URL_FILE)
			{
				tmp = uni_strstr(this_name,UNI_L(":/"));

				if(tmp)
					tmp += 2;
			}
#ifdef SYS_CAP_FILESYSTEM_HAS_DRIVES
			if(tmp && (url_type == URL_FILE) &&
				(uni_isalpha(tmp[0]) && (tmp[1] == ':' || tmp[1] == '|') || tmp[0]=='/' ))
			{
				if (tmp[0]!='/')
					tmp--; // need one slash, grab it from the "://"
				unsigned long tmp_pos = tmp -this_name; // Store postition in case GetServername changes location of buffers
				resolved_name.server_name = GetServerName("localhost", TRUE);
				
				// reconstruct buffer pointers
				this_name = uni_temp_buffer1;
				resolved_path = tmp = this_name + tmp_pos;
				resolved_query = uni_strchr(resolved_path,'?');
				if(resolved_query)
				{
					uni_strlcpy(uni_temp_buffer2, resolved_query, temp_buffer_len);
					resolved_query[0] = '\0'; // separate path and query.
					resolved_query++;
					uni_strlcpy(resolved_query,uni_temp_buffer2,  temp_buffer_len- (resolved_query-this_name)); // copy back into work buffer,moved one character and separated from path
				}
			}
			else
#endif // SYS_CAP_FILESYSTEM_HAS_DRIVES
            if(tmp)
			{
				uni_char *tmp_user = tmp; // points at first character
				uni_char *tmp_pos = tmp_user;
				uni_char *tmp_servername = NULL; // points at character before
				uni_char *tmp_pass = NULL; // points at character before
				uni_char *tmp_port = NULL; // points at character before
		
#ifdef SUPPORT_LITERAL_IPV6_URL
				uni_char *ip6_begin = uni_strchr(tmp, '[');
				uni_char *ip6_end = (ip6_begin ? uni_strchr(tmp, ']') : NULL);
				if(ip6_begin && !ip6_end)
					ip6_begin = NULL;
#endif //SUPPORT_LITERAL_IPV6_URL

				while (*tmp_pos != '\0' && *tmp_pos != '/' && *tmp_pos != ';' && *tmp_pos != '#' && *tmp_pos != '?'  )
				{
					if(*tmp_pos == ':' || ( *tmp_pos == '%' && tmp_pos[1] == '3' && (tmp_pos[2] == 'a' || tmp_pos[2] == 'A')))
					{
#ifdef SUPPORT_LITERAL_IPV6_URL
						if( ip6_begin && ip6_begin < tmp_pos )
						{
							if( ip6_end  && tmp_pos > ip6_end )
								tmp_port = tmp_pos;
						}
						else
#endif //SUPPORT_LITERAL_IPV6_URL
						if(tmp_servername == NULL)
						{
							if(tmp_pass == NULL)
								tmp_pass = tmp_pos;
						}
						else
						{
							if(tmp_port == NULL)
								tmp_port = tmp_pos;
						}
					}
					else if(*tmp_pos == '@'  || ( *tmp_pos == '%' && tmp_pos[1] == '4' && tmp_pos[2] == '0'))
					{
						tmp_servername = tmp_pos;
						if(tmp_port)
							tmp_pass = tmp_port;
						tmp_port = NULL;
					}
					tmp_pos++;
				}

				uni_char sep;
						
				if(tmp_servername) // we have a username and/or password
				{
					username = tmp_user;
					if(tmp_pass)
					{
						sep = *tmp_pass;
						*tmp_pass = '\0';
						tmp_pass += (sep == '%'? 3 : 1);
					}
					password = tmp_pass;

					sep = *tmp_servername;
					*tmp_servername= '\0';
					tmp_servername += (sep == '%'? 3 : 1);
				}
				else
				{
					tmp_servername = tmp;
					tmp_port = tmp_pass;
					tmp_pass = NULL;
				}

				uni_char *srv_end = tmp_pos;

				sep = *srv_end;
				*srv_end = '\0';

				// Prevent loss of pointers if GetServerName have to expand the  buffers
				unsigned long pos_user = (username ? username - this_name : 0);
				unsigned long pos_pass = (password ? password - this_name : 0);
				unsigned long pos_srv_end = srv_end - this_name;

				OP_STATUS op_err = OpStatus::OK;
				resolved_name.server_name = GetServerName(op_err, tmp_servername, resolved_name.port, TRUE, (url_type != URL_WIDGET && url_type != URL_MOUNTPOINT && url_type != URL_FILE));

				RETURN_IF_ERROR(op_err);
				if(resolved_name.server_name == NULL) // Error
					return OpStatus::ERR;

				if ((url_type == URL_HTTP || url_type == URL_WEBSOCKET) && resolved_name.port == 80)
					resolved_name.port = 0;
				else if (url_type == URL_WEBSOCKET_SECURE && resolved_name.port == 443)
					resolved_name.port = 0;

				if(this_name != uni_temp_buffer1)
				{
					// Temporary buffers updated by GetServerName, have to refresh the pointers into uni_temp_buffer1
					this_name = uni_temp_buffer1;
					if(pos_user)
						username = this_name + pos_user;
					if(pos_pass)
						password = this_name + pos_pass;
					if(pos_srv_end)
						srv_end = this_name + pos_srv_end;
				}

				*srv_end = sep;

				resolved_path = srv_end;
				resolved_query = uni_strchr(resolved_path,'?');
				if(resolved_query)
				{
					uni_strlcpy(uni_temp_buffer2, resolved_query, temp_buffer_len);
					resolved_query[0] = '\0'; // separate path and query.
					resolved_query++;
					uni_strlcpy(resolved_query,uni_temp_buffer2,  temp_buffer_len- (resolved_query-this_name)); // copy back into work buffer,moved one character and separated from path
				}
			}
		}
		
		if(resolved_path == NULL)
		{
			if(url_type != URL_NULL_TYPE && url_type != URL_UNKNOWN)
			{
				resolved_path = uni_strchr(this_name, ':'); // MUST exist as type is not URL_NULL_TYPE
				OP_ASSERT(resolved_path);
				if(resolved_path)
				{
					resolved_path ++;
					if(url_type != URL_JAVASCRIPT && url_type != URL_DATA)
						resolved_query = uni_strchr(resolved_path,'?');
				}
				if(resolved_query)
				{
					uni_strlcpy(uni_temp_buffer2, resolved_query, temp_buffer_len);
					resolved_query[0] = '\0'; // separate path and query.
					resolved_query++;
					uni_strlcpy(resolved_query,uni_temp_buffer2,  temp_buffer_len- (resolved_query-this_name)); // copy back into work buffer,moved one character and separated from path
				}
			}
			else
			{
				OpStringC8 prnt_path1 = prnt_url->GetAttribute(URL::KPath); 
				uni_temp_buffer2[0] = '\0';
				uni_char *prnt_path = uni_temp_buffer2 +1; 
				make_doublebyte_in_buffer(prnt_path1.CStr(), prnt_path1.Length() , prnt_path , temp_buffer_len-1);

				if(*this_name == '?')
				{
					int prnt_len = uni_strlen(prnt_path);
					uni_char *temp_query = prnt_path+prnt_len+2;
					uni_strlcpy(temp_query, this_name, temp_buffer_len-prnt_len-2); // copy into buffer after parent path
										
					//uni_strcat(prnt_path, this_name); // append query
					//uni_temp_buffer1[0] = '\0'; // Zeropad before 
					//uni_strcpy(uni_temp_buffer1+1, prnt_path);
					uni_temp_buffer1[0] = '\0'; // Zeropad before 
					uni_strlcpy(uni_temp_buffer1+1, prnt_path, temp_buffer_len-2);

					resolved_path = uni_temp_buffer1+1;

					resolved_query = resolved_path+ uni_strlen(resolved_path)+2;

					uni_strlcpy(resolved_query, temp_query, temp_buffer_len-(temp_query - uni_temp_buffer1)); // copy into buffer after parent path
				}
				else
				{
					resolved_path = this_name;
					resolved_query = uni_strchr(resolved_path,'?');
					if(resolved_query)
					{
						uni_strlcpy(uni_temp_buffer2, resolved_query, temp_buffer_len);
						resolved_query[0] = '\0'; // separate path and query.
						resolved_query++;
						uni_strlcpy(resolved_query,uni_temp_buffer2,  temp_buffer_len- (resolved_query-this_name)); // copy back into work buffer,moved one character and separated from path
					}
				}
			}
		}

#ifdef _LOCALHOST_SUPPORT_
#ifdef SYS_CAP_FILESYSTEM_HAS_DRIVES
		OP_ASSERT(resolved_path);
		if((url_type == URL_FILE) && resolved_path[0])
		{
			OP_ASSERT(resolved_path[0] == '/');
			if(resolved_name.server_name != NULL &&
				OpStringC8(resolved_name.server_name->Name()).Compare("localhost") == 0 && 
				resolved_path[1] == '/')
			{
				OpStringC8 empty_string;
				OpStringC8 ref_path = (prnt_url ? prnt_url->GetAttribute(URL::KPath) : empty_string);
				if(purl_type == URL_FILE && ref_path.HasContent() && (ref_path[2] == ':' || ref_path[2] == '|'))
				{
					// Copy drive
					uni_temp_buffer2[0] = ref_path[1];
					uni_temp_buffer2[1] = ref_path[2];
				}
				else
				{
					// get drive from system
					OpString tmp_storage;
					const uni_char* tmp = g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_HOME_FOLDER, tmp_storage);
					
					if (tmp && uni_isalpha(*tmp) && tmp[1] == ':')
					{
						uni_strncpy(uni_temp_buffer2, tmp,2);
						// NUL termination not needed; copy operation continues further down
					}
					else
					{
# ifdef SYS_CAP_FILESYSTEM_HAS_MULTIPLE_DRIVES
						uni_char curdrive = g_op_system_info->GetCurrentDriveLetter();
						if (!curdrive)
							curdrive = 'C';
# else // SYS_CAP_FILESYSTEM_HAS_MULTIPLE_DRIVES
						const uni_char curdrive = 'C';
# endif
						uni_temp_buffer2[0] = curdrive;
						uni_temp_buffer2[1] = ':';
					}
				}
				// insert drive letter
				uni_strcpy(uni_temp_buffer2+2, resolved_path);
				uni_strcpy(resolved_path, uni_temp_buffer2);
			}			
		}
#endif // SYS_CAP_FILESYSTEM_HAS_DRIVES
#endif // _LOCALHOST_SUPPORT_
    }
    else if (prnt_len)
    {
		resolved_name.scheme_spec = (const protocol_selentry *) prnt_url->GetAttribute(URL::KProtocolScheme, NULL);
		resolved_name.url_type = url_type = (URLType) prnt_url->GetAttribute(URL::KRealType);
		resolved_name.username = prnt_url->GetAttribute(URL::KUserName).CStr();
		resolved_name.password = prnt_url->GetAttribute(URL::KPassWord).CStr();
		resolved_name.server_name = (ServerName *) prnt_url->GetAttribute(URL::KServerName, NULL);
		resolved_name.port = (unsigned short) prnt_url->GetAttribute(URL::KServerPort);


		if(purl_type == URL_HTTP || purl_type == URL_HTTPS || purl_type == URL_FTP)
		{
			const char *urltyp = resolved_name.scheme_spec->protocolname;
			if(urltyp)
			{
				int len  = op_strlen(urltyp);

				if(uni_strni_eq_lower(this_name, urltyp, len) && this_name[len] == ':')
					this_name+=len+1;
			}
		}
		
        if (*this_name == '/' || purl_type == URL_NEWS || purl_type == URL_SNEWS)
        {
			resolved_path = this_name;
			resolved_query = uni_strchr(resolved_path,'?');
			if(resolved_query)
			{
				uni_strlcpy(uni_temp_buffer2, resolved_query, temp_buffer_len);
				resolved_query[0] = '\0'; // separate path and query.
				resolved_query++;
				uni_strlcpy(resolved_query,uni_temp_buffer2,  temp_buffer_len- (resolved_query-this_name)); // copy back into work buffer,moved one character and separated from path
			}
		}
		else if(*this_name == '\0')
		{
			resolved_name.path = prnt_url->GetAttribute(URL::KPath).CStr(); 
			resolved_name.query = prnt_url->GetAttribute(URL::KQuery).CStr(); 
		}
		else
		{
			OpStringC8 prnt_path1 = prnt_url->GetAttribute(URL::KPath); 
			uni_temp_buffer2[0] = '\0';
			uni_char *prnt_path = uni_temp_buffer2 +1; 
			make_doublebyte_in_buffer(prnt_path1.CStr(), prnt_path1.Length() , prnt_path , temp_buffer_len-1);

			//uni_char* end_p = (purl_type == URL_HTTP || purl_type == URL_HTTPS || url_type == URL_FILE) ? (uni_char*) uni_strchr(prnt_path, '?') : 0;

			//if(end_p)
			//	*end_p = '\0';

			if(*this_name == '?')
			{
				int prnt_len = uni_strlen(prnt_path);
				uni_char *temp_query = prnt_path+prnt_len+2;
				uni_strlcpy(temp_query, this_name, temp_buffer_len-prnt_len-2); // copy into buffer after parent path
									
				//uni_strcat(prnt_path, this_name); // append query
				//uni_temp_buffer1[0] = '\0'; // Zeropad before 
				//uni_strcpy(uni_temp_buffer1+1, prnt_path);
				uni_temp_buffer1[0] = '\0'; // Zeropad before 
				uni_strlcpy(uni_temp_buffer1+1, prnt_path, temp_buffer_len-2);

				resolved_path = uni_temp_buffer1+1;

				resolved_query = resolved_path+ uni_strlen(resolved_path)+2;

				uni_strlcpy(resolved_query, temp_query, temp_buffer_len-(temp_query - uni_temp_buffer1)); // copy into buffer after parent path
			}
			else
			{
				uni_char *end_p = uni_strrchr(prnt_path, '/');
				
#if PATHSEPCHAR == '\\'
				// Some user enter file urls with \'s
				if(purl_type == URL_FILE)
				{
					uni_char *temp_end = uni_strchr((end_p ? end_p : prnt_path), '\\');
					if(temp_end)
						end_p = temp_end;
				}
#endif
				if(!end_p)
				{
					end_p = prnt_path;
					*end_p = '/';
				}

				end_p ++;
				uni_strcpy(end_p, this_name);
				
				uni_temp_buffer1[0] = '\0'; // Zeropad before 
				uni_strcpy(uni_temp_buffer1+1, prnt_path);

				resolved_path = uni_temp_buffer1+1;
				resolved_query = uni_strchr(resolved_path,'?');
				if(resolved_query)
				{
					uni_strlcpy(uni_temp_buffer2, resolved_query, temp_buffer_len);
					resolved_query[0] = '\0'; // separate path and query.
					resolved_query++;
					uni_strlcpy(resolved_query,uni_temp_buffer2,  temp_buffer_len- (resolved_query-this_name)); // copy back into work buffer,moved one character and separated from path
				}
			}
        }
    }
    else
	{
		resolved_name.url_type = url_type = URL_UNKNOWN;
		resolved_path = this_name;
	}

	if(resolved_path && url_type != URL_JAVASCRIPT && url_type != URL_DATA && url_type != URL_ED2K)
	{

		if(!g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseUTF8Urls))
		{
			UriEscape::Escape(uni_temp_buffer2, resolved_path, UriEscape::Range_80_ff);
			uni_strcpy(resolved_path, uni_temp_buffer2);
		}

		//if(url_type != URL_FILE)
		{
			// unescape ALL encoded "/" and "." sections (%2F and %2E are turned 
			// into "/" and "." permanently), before the query part of the URL.
			// This makes parsing (and removing) /./ and /../ securely much easier.
			// Also, RFC 3986 specified that unreserved characters, of which "." is 
			// a part should always be unescaped, and we do the same to "/". Other 
			// unreserved characters are currently not unescaped.
			// File URLs are not included, as there is no real danger involved for files.
			uni_char *src = resolved_path;
			uni_char *trg = resolved_path;

			while (*src  /*&& *src != '?'*/)
			{
				uni_char token = *(src++);

				if(token == '/' || (token == '%' && src[0] == '2' && (src[1] | 0x20) == 'f'))
				{
					do{
						size_t pos = (token == '%' ? 2 : 0);

						uni_char temp_token = src[pos++];
						if(!temp_token /*|| temp_token == '?'*/ || !(temp_token == '.' || (temp_token == '%' && src[pos] == '2' && (src[pos+1] | 0x20) == 'e')))
							break;

						if(temp_token == '%')
							pos+=2;

						temp_token = src[pos++];
						if(temp_token == '/' || (temp_token == '%' && src[pos] == '2' && (src[pos+1] | 0x20) == 'f'))
						{
							if(temp_token == '%')
								pos+=2;
							//write "/." and check for next "/" event
							*(trg++) = '/';
							*(trg++) = '.';
							token = '/';
							src += pos;
							pos = 0;

							continue; // check if the "/" is part of a new dot-segment
						}
						else if(!temp_token /*|| temp_token == '?'*/)
						{
							// Write "/."
							*(trg++) = '/';
							token = '.';
							src += pos-1;

							break;
						}
						else if (!(temp_token == '.' || (temp_token == '%' && src[pos] == '2' && (src[pos+1] | 0x20) == 'e')))
						{
							break;
						}

						if(temp_token == '%')
							pos+=2;

						temp_token = src[pos++];
						if(temp_token == '/' || (temp_token == '%' && src[pos] == '2' && (src[pos+1] | 0x20) == 'f'))
						{
							if(temp_token == '%')
								pos+=2;
							//write "/.." and check for next "/" event
							*(trg++) = '/';
							*(trg++) = '.';
							*(trg++) = '.';
							token = '/';
							src += pos;
							pos = 0;

							continue; // check if the "/" is part of a new dot-segment
						}

						if(!temp_token /* || temp_token == '?'*/)
						{
							// Write "/.."
							*(trg++) = '/';
							*(trg++) = '.';
							token = '.';
							src += pos-1;

							break;
						}
						else 
						{
							// Not a dot segment
							break;
						}
					}while(token == '/');
				}

				*(trg++) = token;
			}

			while ((*(trg++) = *(src++)) != '\0'){};
		}
		// Remove parent-dir and same-dir references in the path component
		unsigned long i=0;
		while (resolved_path[i] != '\0' /*&& resolved_path[i] != '?'*/)
		{
			if (resolved_path[i] == '/' && resolved_path[i+1] == '.')
			{
				unsigned long  j = i;
				
				do{

					if (resolved_path[i+2] == '.' && (resolved_path[i+3] == '/' || resolved_path[i+3] == '\0'))
					{
#ifndef NO_FTP_SUPPORT
						// Do not remove leading ../ in FTP url unless absolute paths are used
						if(url_type == URL_FTP &&
						   !g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseAbsoluteFTPPath) )
						{
							if (i>= 3 && resolved_path[i-3] == '/' && resolved_path[i-2] == '.' && resolved_path[i-1] == '.' ) // else we have reached host name
							{
								i+=3;
								continue;
							}
							else if (j > 0) // else we have reached host name
								while (--j > 0 && resolved_path[j] != '/')
								{
								}
						}
						else
#endif // NO_FTP_SUPPORT
						{
							if (j > 0) // else we have reached host name
								while (--j > 0 && resolved_path[j] != '/')
								{
								}
						}
						i += 3;
					}
					else if (resolved_path[i+2] == '/')
					{
						i += 2;
					}
					/* else if (resolved_path[i+2] == '?' )
					{
						j++;
						i+=2;
						break;
					}*/
					else if (resolved_path[i+2] == 0 )
					{
						OP_ASSERT(resolved_path[j] == '/'); // Should never happen
						i+=2;
						break;
					}
					else
					{
						break;
					}
				}while(resolved_path[i] == '/' && resolved_path[i+1] == '.');
				
				if (/*j >= 0 &&*/ j != i)
				{
					int k = 0;
					while (resolved_path[i+k] != '\0')
					{
						resolved_path[j+k] = resolved_path[i+k];
						k++;
					}
					if(k == 0 && j+1 != i)
						k++;
					resolved_path[j+k] = '\0';
					i = j;
				}
				else
					i += 2;
			}
			else
				i++;
		}
	}

	unsigned long user_pos = 0; 
	unsigned long pass_pos = 0; 
	unsigned long path_pos = 0; 
	unsigned long query_pos = 0; 

	temp_buffer2[0] = '\0';
	unsigned long work_pos = 1; 
	{
		// These makes it easier to perform the operation
		OpStringC16 plain_user(username);
		OpStringC16 plain_password(password);
		OpStringC16 plain_path(resolved_path);
		OpStringC16 plain_query(resolved_query);

		if(plain_user.CStr()) // some content at least
		{
			user_pos = work_pos;
			if(utf8_escapes)
				work_pos += plain_user.UTF8(temp_buffer2+ user_pos, temp_buffer_len- work_pos);
			else
			{
				work_pos += uni_cstrlcpy(temp_buffer2+ user_pos, plain_user.CStr(), temp_buffer_len- work_pos);
			}
			OP_ASSERT(work_pos <= temp_buffer_len);
			temp_buffer2[work_pos++] = '\0';
		}
		if(plain_password.CStr()) // some content at least
		{
			pass_pos = work_pos;
			if(utf8_escapes)
				work_pos += plain_password.UTF8(temp_buffer2+ pass_pos, temp_buffer_len- work_pos);
			else
			{
				work_pos += uni_cstrlcpy(temp_buffer2+ pass_pos, plain_password.CStr(), temp_buffer_len- work_pos);
			}
			OP_ASSERT(work_pos <= temp_buffer_len);
			temp_buffer2[work_pos++] = '\0';
		}
		if(plain_path.CStr()) // some content at least
		{
			path_pos = work_pos;
			if(utf8_escapes)
				work_pos += plain_path.UTF8(temp_buffer2+ path_pos, temp_buffer_len- work_pos);
			else
			{
				work_pos += uni_cstrlcpy(temp_buffer2+ path_pos, plain_path.CStr(), temp_buffer_len- work_pos);
			}
			OP_ASSERT(work_pos <= temp_buffer_len);
			temp_buffer2[work_pos++] = '\0';
			work_pos +=2; // Make space for slashed path
		}
		if(plain_query.CStr()) // some content at least
		{
			query_pos = work_pos;
			if(utf8_escapes)
				work_pos += plain_query.UTF8(temp_buffer2+ query_pos, temp_buffer_len- work_pos);
			else
			{
				work_pos += uni_cstrlcpy(temp_buffer2+ query_pos, plain_query.CStr(), temp_buffer_len- work_pos);
			}
			OP_ASSERT(work_pos <= temp_buffer_len);
			temp_buffer2[work_pos++] = '\0';
		}
	}

	unsigned long escapes = 0;

	// don't escape a javascript url !!!
	if (url_type != URL_JAVASCRIPT && url_type != URL_DATA && url_type != URL_ED2K)
	{
		if(user_pos)
			escapes += UriEscape::CountEscapes(temp_buffer2+ user_pos, UriEscape::StandardUnsafe);
		if(pass_pos)
			escapes += UriEscape::CountEscapes(temp_buffer2+ pass_pos, UriEscape::StandardUnsafe);
		if(path_pos)
			escapes += UriEscape::CountEscapes(temp_buffer2+ path_pos, UriEscape::StandardUnsafe);
		if(query_pos)
			escapes += UriEscape::CountEscapes(temp_buffer2+ query_pos, UriEscape::QueryStringUnsafe);
	}

	// Calculates maximum length of the URL.
	RETURN_IF_ERROR(CheckTempbuffers(OpStringC8(resolved_name.username).Length()+
			OpStringC8(resolved_name.password).Length()+
			OpStringC8(resolved_name.server_name  != NULL? resolved_name.server_name->Name() : NULL).Length() +
			OpStringC8(resolved_name.path).Length()+
			OpStringC8(resolved_name.query).Length()+
			work_pos + escapes*2+256));

	temp_buffer1[0] = 0;
	work_pos = 0;
	char *slashed_path = NULL;

	if (escapes)
	{
		if(user_pos)
		{
			UriEscape::Escape(temp_buffer1+work_pos+1, temp_buffer2+ user_pos, UriEscape::StandardUnsafe);
			user_pos = work_pos+1;
			work_pos += op_strlen(temp_buffer1+ user_pos) +1;
		}
		if(pass_pos)
		{
			UriEscape::Escape(temp_buffer1+work_pos+1, temp_buffer2+ pass_pos, UriEscape::StandardUnsafe);
			pass_pos = work_pos+1;
			work_pos += op_strlen(temp_buffer1+ pass_pos) +1;
		}
		if(path_pos)
		{
			slashed_path = temp_buffer1+work_pos+1;
			*slashed_path = '/';
			work_pos++;
			UriEscape::Escape(temp_buffer1+work_pos+1, temp_buffer2+ path_pos, UriEscape::StandardUnsafe);
			path_pos = work_pos+1;
			work_pos += op_strlen(temp_buffer1+ path_pos) +1;
		}
		if(query_pos)
		{
			UriEscape::Escape(temp_buffer1+work_pos+1, temp_buffer2+ query_pos, UriEscape::QueryStringUnsafe);
			query_pos = work_pos+1;
			work_pos += op_strlen(temp_buffer1+ query_pos) +1;
		}
	}
	else
	{
		if(user_pos)
			op_strcpy(temp_buffer1+user_pos, temp_buffer2+user_pos);
		if(pass_pos)
			op_strcpy(temp_buffer1+pass_pos, temp_buffer2+pass_pos);
		if(path_pos)
		{
			slashed_path = temp_buffer1+path_pos;
			*slashed_path = '/';
			op_strcpy(temp_buffer1+path_pos+1, temp_buffer2+path_pos);
			path_pos ++;
		}
		if(query_pos)
			op_strcpy(temp_buffer1+query_pos, temp_buffer2+query_pos);
	}

	if(user_pos)
		resolved_name.username = temp_buffer1+user_pos;
	if(pass_pos)
		resolved_name.password = temp_buffer1+pass_pos;
	if(path_pos)
		resolved_name.path = temp_buffer1+path_pos;
	if(query_pos)
		resolved_name.query = temp_buffer1+query_pos;

	if(url_type == URL_HTTP || url_type == URL_HTTPS ||
			url_type == URL_NEWS || url_type == URL_SNEWS ||
			url_type == URL_FTP || url_type == URL_FILE ||
			url_type == URL_WEBSOCKET || url_type == URL_WEBSOCKET_SECURE)
	{
		if(!resolved_name.path || !*resolved_name.path)
			resolved_name.path = "/";
		else if(slashed_path && *resolved_name.path != '/')
			resolved_name.path = slashed_path;
	}

	if(url_type == URL_UNKNOWN || url_type == URL_NULL_TYPE )
	{
		resolved_name.scheme_spec = NULL;
		url_type = prnt_url ? (URLType) prnt_url->GetAttribute(URL::KRealType) : URL_NULL_TYPE;

		if (url_type == URL_OPERA)
			resolved_name.scheme_spec = GetUrlScheme(UNI_L("opera"));
	}
	else if(url_type == URL_ABOUT)
	{
		if ( !resolved_name.path || (op_strcmp(resolved_name.path, "blank") != 0 && op_strcmp(resolved_name.path, "streamurl") != 0) )
			resolved_name.scheme_spec = GetUrlScheme(UNI_L("opera"));
	}

	OP_STATUS op_err = OpStatus::OK;
	FindSchemeAndAuthority(op_err, &resolved_name, TRUE);
	RETURN_IF_ERROR(op_err);
	resolved_name.OutputString(URL::KName_Username_Password_Escaped_NOT_FOR_UI, temp_buffer2, temp_buffer_len, URL_NAME_LinkID);

	resolved_name.full_url = temp_buffer2;

	return OpStatus::OK;
}

//	___________________________________________________________________________
//	ResolveUrlNameL
//	Make sure we have the localhost, www etc. part in the URL name
//	This is basically a copy and past from GetURL below.
//	___________________________________________________________________________
//
BOOL ResolveUrlNameL(const OpStringC& szNameIn,OpString& szResolved, BOOL entered_by_user)
{
	//
	//	Make a copy of the name to resolve
	//	pszSource will be the string we are reading from and this will point to
	//	a temp. allocated buffer inside this function (due to the \ => / conversion)
	//
	OpString pszSource; ANCHOR(OpString,pszSource);
	pszSource.SetL(szNameIn);
	pszSource.Strip();

	if(pszSource.IsEmpty())
	{
		return FALSE;
	}

	if (!uni_strni_eq(pszSource.CStr(), "JAVASCRIPT:", 11) &&
		!uni_strni_eq(pszSource.CStr(), "DATA:", 5) &&
		!uni_strni_eq(pszSource.CStr(), "ED2K:", 5) &&
		!uni_strni_eq(pszSource.CStr(), "MAILTO:", 7))
	{
		// Not for Javascript URLs or data URLs
		//
		//	Replace \ with /
		//
		for (int j = 0; pszSource[j]; j++)
		{
			if (pszSource[j] == '\\')
			{
				pszSource[j] = '/';
			}
			else if(pszSource[j] == '?')
				break; // don't continue into query component
		}
	}

	//
	//	Hmmm
	//
#ifdef SYS_CAP_FILESYSTEM_HAS_DRIVES
	// Looks for eg C:.  Ifdef should be feature, not platform
	if (uni_isalpha(pszSource[0]) && pszSource[1] == ':')
		// Found drive letter
		szResolved.SetL(UNI_L("file://localhost/")).AppendL(pszSource);
#else // SYS_CAP_FILESYSTEM_HAS_DRIVES
	if (pszSource[0] && pszSource[0] == '/' && pszSource[1] != '/')
		// Found local file
		szResolved.SetL(UNI_L("file://localhost")).AppendL(pszSource);
#endif // SYS_CAP_FILESYSTEM_HAS_DRIVES
#ifdef SYSTEMINFO_ISLOCALFILE
	else if ( g_op_system_info->IsLocalFile( pszSource))
		szResolved.SetL(UNI_L("file://localhost/")).AppendL(pszSource);
#endif // SYSTEMINFO_ISLOCALFILE
	else if (!pszSource.Compare(UNI_L("//"), 2))
		// Found remote drive
		szResolved.SetL(UNI_L("file:")).AppendL(pszSource);
	else if (uni_strni_eq(pszSource.CStr(), "WWW.", 4) || uni_strni_eq(pszSource.CStr(), "WWW", 4))
		szResolved.SetL(UNI_L("http://")).AppendL(pszSource);
#ifndef NO_FTP_SUPPORT
	else if (uni_strni_eq(pszSource.CStr(), "FTP.", 4) || uni_strni_eq(pszSource.CStr(), "FTP", 4))
		szResolved.SetL("ftp://").AppendL(pszSource);
#endif // NO_FTP_SUPPORT
	else if (uni_strni_eq(pszSource.CStr(), "GOPHER.", 7) || uni_strni_eq(pszSource.CStr(), "GOPHER", 7))
		szResolved.SetL(UNI_L("gopher://")).AppendL(pszSource);
	else if (uni_strni_eq(pszSource.CStr(), "WAIS.", 5) || uni_strni_eq(pszSource.CStr(), "WAIS", 5))
		szResolved.SetL(UNI_L("wais://")).AppendL(pszSource);
	
	else if (uni_strni_eq(pszSource.CStr(), "FILE:", 5) &&	!uni_strni_eq(pszSource.CStr(), "FILE:/", 6))
		LEAVE_IF_ERROR(szResolved.SetL(UNI_L("file://localhost/")).Append(pszSource.SubString(5)));
	else if (uni_strni_eq(pszSource.CStr(), "FILE:/", 6) &&	!uni_strni_eq(pszSource.CStr(), "FILE://", 7))
		LEAVE_IF_ERROR(szResolved.SetL(UNI_L("file://localhost/")).Append(pszSource.SubString(6)));
	else if (uni_strni_eq(pszSource.CStr(), "FILE:///", 8) && !uni_strni_eq(pszSource.CStr(), "FILE:////", 9))
		LEAVE_IF_ERROR(szResolved.SetL(UNI_L("file://localhost/")).Append(pszSource.SubString(8)));
#ifdef SUPPORT_LITERAL_IPV6_URL
	else if( pszSource[0] && pszSource[0] == '[' )
		// Found literal IPv6 address
		szResolved.SetL(UNI_L("http://")).AppendL(pszSource);
#endif // SUPPORT_LITERAL_IPV6_URL
#if !defined(NO_URL_OPERA) || defined(HAS_OPERABLANK)
	else if (uni_strni_eq(pszSource.CStr(), "ABOUT:", 7) || uni_strni_eq(pszSource.CStr(), "ABOUT:OPERA", 12))
	{
		szResolved.SetL("opera:about");
	}
#endif // NO_URL_OPERA
	else
	{
		uni_char *this_name = pszSource.CStr();

		uni_char *tmp = uni_strchr(this_name, ':');
		URLType url_type = URL_NULL_TYPE;
		if(tmp)
		{
			*tmp = 0;
			url_type = urlManager->MapUrlType(this_name);
			if(url_type == URL_UNKNOWN)
			{
				uni_char *test = this_name;
				while(*test)
				{
					uni_char tc = *test;
					if(tc >= 128 || 
						(!op_isalnum(tc) && tc != '-' && tc != '+' && tc != '.'))
					{
						*tmp = ':';
						tmp = NULL;
						break;
					}

					test ++;
				}
			}
			if(tmp)
			    *tmp = ':';
		}
		switch(url_type)
		{
		case URL_HTTP:
		case URL_HTTPS:
		case URL_FTP:
		case URL_WEBSOCKET:
		case URL_WEBSOCKET_SECURE:
			if(tmp[1] != '/' || tmp[2] != '/')
			{
				OpString temp_scheme;
				ANCHOR(OpString, temp_scheme);

				temp_scheme.SetL(urlManager->MapUrlType(url_type));

				LEAVE_IF_ERROR(szResolved.SetConcat(temp_scheme, UNI_L("://"), tmp+(tmp[1] != '/' ? 1 : 2))); // create a http://url if one or both of the slashes are missing
				break;
			}
			// Fall through to default
		case URL_JAVASCRIPT:
		case URL_DATA:
		default:
			tmp = NULL;
			break;
		case URL_UNKNOWN:
		case URL_NULL_TYPE:
			// If it is unknown and has a digit after the ":", assume that the digit is part of a portnumber, and prepend "http://"
			if(!tmp || uni_isdigit(tmp[1]))
			{
				szResolved.SetL("http://").AppendL(pszSource);
				tmp = szResolved.CStr(); // trick the use original code below.
			}
			else
			{
				tmp = NULL;
			// If not a digit, assume it is an unknown protocol.
#ifdef NEED_URL_HANDLE_UNRECOGNIZED_SCHEME
                HandleUnrecognisedURLTypeL(szNameIn);
#endif
			}
			break;
		}
		
		if(!tmp)
			szResolved.SetL(pszSource);
	}

	// Bug#230124: If this URL was entered by the user, any forms data
	// should be considered entered in the system character encoding.
	// If UTF-8 encoding of URLs has been disabled for the site, the
	// entire local part should be considered encoded in the system
	// character encoding.
#ifdef SYS_CAP_FILESYSTEM_HAS_DRIVES
	/* CORE-49250: Translate | to : in the "drive letter" position for all "file:" urls,
	 * i.e. after they have all been converted to something like "file://localhost/c|"
	 * or "file://c|". The latter case is an exception, since in this form the string
	 * following "file://" can be a remote drive. However, if it looks like a drive
	 * letter, we convert it anyway.
	 */
	if (szResolved.Length() > 18 && uni_strni_eq(szResolved.CStr(), "file://localhost/", 17) &&
		uni_isalpha(szResolved[17]) && szResolved[18] == '|')
	{
		szResolved[18] = ':';
	}
	else if (szResolved.Length() > 8 && uni_strni_eq(szResolved.CStr(), "file://", 7) &&
		uni_isalpha(szResolved[7]) && szResolved[8] == '|')
	{
		szResolved[8] = ':';
	}
#endif

	if (entered_by_user && !uni_strni_eq(szResolved.CStr(), "JAVASCRIPT:", 11) && !uni_strni_eq(szResolved.CStr(), "FILE:", 5))
	{
		// Fetch the system encoding
		const char *system_encoding = NULL;
		TRAPD(rc, system_encoding = g_op_system_info->GetSystemEncodingL());
		if (OpStatus::IsSuccess(rc) &&
			system_encoding && *system_encoding &&
			op_strcmp(system_encoding, "utf-8") != 0 /* skip if already utf-8 */)
		{
			// We should have a full URL here. We need to find either the
			// host name or the forms data, depending on the UTF-8 setting.
			// FIXME: This duplicates the code in CleanUrlName.
			const uni_char *colonslashslash = NULL;
			const uni_char *colon = uni_strchr(szResolved.CStr(), ':');
			uni_char *forms_data = uni_strchr(szResolved.CStr(), '?');
			if (colon)
			{
				if (forms_data && colon > forms_data)
				{
					colon = NULL;
				}
				else
				{
					colonslashslash = uni_strstr(colon, UNI_L("://"));
				}
			}

			// Locate the server name, if any (needed for sitepref checking)
#ifdef PREFS_HOSTOVERRIDE
			OpString hostname;
			if (colonslashslash)
			{
				// Start of hostname is after the "://"
				const uni_char *hostnamestart = colonslashslash + 3;

				// End of hostname is before the first "/", or the first
				// ":", whichever comes first. If neither, it ends at the
				// end of the string.
				const uni_char *hostnameend = uni_strchr(hostnamestart, '/');

				const uni_char *password_end = uni_strchr(hostnamestart, '@');
				while(password_end && password_end < hostnameend)
				{
					hostnamestart = password_end+1;
					password_end = uni_strchr(hostnamestart, '@');
				}
				password_end = uni_strstr(hostnamestart, "%40");
				while(password_end && password_end < hostnameend)
				{
					hostnamestart = password_end+1;
					password_end = uni_strstr(hostnamestart, "%40");
				}

				const uni_char *colon = uni_strchr(hostnamestart, ':');
				if (colon && (!hostnameend || colon < hostnameend))
					hostnameend = colon;
				if (!hostnameend)
				{
					// Point to the end of the string
					hostnameend = hostnamestart + uni_strlen(hostnamestart);
				}
				
				if (hostnameend - hostnamestart > 0)
				{
					OpStatus::Ignore(hostname.Set(hostnamestart, hostnameend - hostnamestart));
				}
			}
#endif

			BOOL encode_only_forms_data =
				g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseUTF8Urls
#ifdef PREFS_HOSTOVERRIDE
				, hostname.CStr()
#endif
				);

			// Find either the start of the forms data, or the start
			// of the server-side path, depending on configuration
			if (encode_only_forms_data)
			{
				if (forms_data)
				{
					++ forms_data; // Skip question mark
				}
			}
			else if (colonslashslash)
			{
				// Locate the server-part of the URL.
				forms_data = (uni_char*)uni_strchr(colonslashslash + 3, '/');
				if (forms_data)
				{
					++ forms_data; // Skip slash
				}
			}

			// Convert the path or forms data, if found
			if (forms_data)
			{
				size_t idx = forms_data - szResolved.CStr();
				// Expand the buffer (this invalidates the forms_data pointer)
				if (szResolved.Reserve(MAX_URL_LEN))
				{
					unsigned int len = szResolved.Length();
					if (idx < len)
					{
						EncodeFormsData(system_encoding, szResolved.CStr(), idx, len, szResolved.Capacity());
						*(szResolved.CStr() + len) = 0;
					}
				}
			}
		}
	}

	return TRUE;
}

URL_Scheme_Authority_Components *URL_Manager::FindSchemeAndAuthority(OP_STATUS &op_err, URL_Name_Components_st *components, BOOL create)
{
	op_err = OpStatus::OK;
	if(!components || !components->scheme_spec)
		return NULL;

	if(components->server_name != NULL)
		return components->server_name->FindSchemeAndAuthority(op_err, components, create);

	return scheme_and_authority_list.FindSchemeAndAuthority(op_err, components, create);
}
