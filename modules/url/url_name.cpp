/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2000-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"

#include "modules/url/url2.h"
#include "modules/url/url_sn.h"
#include "modules/url/url_name.h"

#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/encodings/utility/charsetnames.h"
#include "modules/util/timecache.h"
#include "modules/util/handy.h"
#include "modules/util/gen_str.h"
#include "modules/encodings/utility/opstring-encodings.h"
#include "modules/formats/uri_escape.h"

#include "modules/url/uamanager/ua.h"
#include "modules/hardcore/mem/mem_man.h"

#include "modules/url/prot_sel.h"

#ifdef SYS_CAP_FILENAMES_IN_SYSTEM_CODEPAGE
#include "modules/encodings/detector/charsetdetector.h"
#endif

#include "modules/encodings/encoders/utf8-encoder.h"
#include "modules/encodings/decoders/utf8-decoder.h"
//#include "comm.h"
#include "modules/url/url2.h"
#include "modules/url/url_rel.h"
#include "modules/url/url_man.h"
#include "modules/cookies/url_cm.h"
#if defined(_SSL_SUPPORT_) && !defined(_EXTERNAL_SSL_SUPPORT_) 
//#include "modules/libssl/sslv3.h"
#endif

#include "modules/olddebug/tstdump.h"

#ifdef _OPERA_DEBUG_DOC_
unsigned long URL_Name::GetMemUsed()
{
	return path.Length()+1;
}
#endif

#ifdef YNP_DEBUG_NAME
Head url_name_list;
#endif


static BOOL check_outputstring_attribute(URL::URL_NameStringAttribute password_hide)
{
	switch (password_hide) {
		case URL::KNoNameStr:
		case URL::KName:
		case URL::KName_Username:
		case URL::KName_Username_Password_NOT_FOR_UI:
		case URL::KName_Username_Password_Hidden:
		case URL::KName_With_Fragment:
		case URL::KName_With_Fragment_Username:
		case URL::KName_With_Fragment_Username_Password_NOT_FOR_UI:
		case URL::KName_With_Fragment_Username_Password_Hidden:
			return FALSE;
		default:
			return TRUE;
	}
}

void URL_Name_Components_st::OutputString(URL::URL_NameStringAttribute password_hide, char *output_buffer, size_t buffer_len, int linkid) const
{
	if (buffer_len == 0)
		return;

	output_buffer[0] = '\0';

	if (check_outputstring_attribute(password_hide) == FALSE)
	{
		OP_ASSERT(!"Only escaped urls are allowed.");
		return;
	}

	if(basic_components != NULL)
		basic_components->OutputString(password_hide, output_buffer, buffer_len, linkid);

	int current_len =  op_strlen(output_buffer);
	if(path && buffer_len - current_len > 0)
		op_strncat(output_buffer, path, buffer_len - current_len);

	current_len =  op_strlen(output_buffer);

	if(query && buffer_len - current_len > 0)
		op_strncat(output_buffer, query, buffer_len - current_len);

	output_buffer[buffer_len - 1] = '\0';
}

void URL_Name_Components_st::OutputString(URL::URL_NameStringAttribute password_hide, uni_char *output_buffer, size_t buffer_len) const
{
	output_buffer[0] = '\0';
	if (check_outputstring_attribute(password_hide) == FALSE)
	{
		OP_ASSERT(!"Only escaped urls are allowed.");
		return;
	}

	if(basic_components != NULL)
		basic_components->OutputString(static_cast<URL::URL_UniNameStringAttribute>(password_hide), output_buffer, buffer_len);

	size_t len =  uni_strlen(output_buffer);
	OP_ASSERT(len < buffer_len);
	size_t rest_len = buffer_len - len;

	OP_ASSERT(!path || rest_len > op_strlen(path));
	/* path and query are 8 bit strings, so no utf8 decoding is needed */
	if (path && rest_len)
	{
		size_t path_len = op_strlen(path);
		if (path_len >= rest_len)
			return;

		make_doublebyte_in_buffer(path, path_len, output_buffer + len, rest_len);
		rest_len = rest_len - (MIN(path_len, rest_len));
		len += path_len;
	}

	OP_ASSERT(rest_len < buffer_len);
	OP_ASSERT(!query || rest_len > op_strlen(query));
	if (query && rest_len)
	{
		size_t query_len = op_strlen(query);
		if (query_len >= rest_len)
			return;

		make_doublebyte_in_buffer(query, query_len, output_buffer + len, rest_len);
	}
}

URL_Name::URL_Name()
{
#ifdef YNP_DEBUG_NAME
	Into(&url_name_list);
#endif
}

OP_STATUS URL_Name::CheckBufferSize(unsigned long templen) const
{
	if (g_tempurl_len < templen+256)
	{
        // round length upward to nearest multiple of 256 and add 256
		size_t new_tempurl_len = (templen < UINT_MAX -256 ? (templen | 0xff) + 0x101 : UINT_MAX-1);
		
		last_user = NULL;
		last_rel_rep = NULL;
		last_hide = URL::KNoStr;
		char *new_tempurl = OP_NEWA(char, new_tempurl_len+1);
		if(new_tempurl == NULL)
			return OpStatus::ERR_NO_MEMORY;

		if(g_tempurl)
			op_memcpy(new_tempurl, g_tempurl, g_tempurl_len);
		
		OP_DELETEA(g_tempurl);
		g_tempurl = new_tempurl;
		// Note: We have to resize g_tempuniurl before we set g_tempurl_len.

		uni_last_user = NULL;
		uni_last_rel_rep = NULL;
		uni_last_hide = URL::KNoUniStr;
		uni_char *new_tempuniurl = OP_NEWA(uni_char, new_tempurl_len+1);
		if(new_tempuniurl == NULL)
			return OpStatus::ERR_NO_MEMORY;

		if(g_tempuniurl)
			op_memcpy(new_tempuniurl, g_tempuniurl, UNICODE_SIZE(g_tempurl_len));

		OP_DELETEA(g_tempuniurl);
		g_tempuniurl = new_tempuniurl;
		g_tempurl_len = new_tempurl_len;

		if (new_tempurl_len > TEMPBUFFER_SHRINK_LIMIT)
			urlManager->PostShrinkTempBufferMessage();
	}

	return OpStatus::OK;
}

/* static */ void URL_Name::ShrinkTempBuffers()
{
	// Grow buffer if it's too small and shrink it if something made
	// it unnecessarily big.
	if (g_tempurl_len > TEMPBUFFER_SHRINK_LIMIT)
	{
		// Must be rounded upward to nearest multiple of 256
		OP_ASSERT(TEMPBUFFER_SHRINK_LIMIT == ((TEMPBUFFER_SHRINK_LIMIT + 255) & ~255));

		last_user = NULL;
		last_rel_rep = NULL;
		last_hide = URL::KNoStr;
		char *new_tempurl = OP_NEWA(char, TEMPBUFFER_SHRINK_LIMIT+1);
		uni_char *new_tempuniurl = OP_NEWA(uni_char, TEMPBUFFER_SHRINK_LIMIT+1);
		if(new_tempurl == NULL || new_tempuniurl == NULL)
		{
			OP_DELETEA(new_tempurl);
			OP_DELETEA(new_tempuniurl);
			return;
		}

		OP_DELETEA(g_tempurl);
		g_tempurl = new_tempurl;

		uni_last_user = NULL;
		uni_last_rel_rep = NULL;
		uni_last_hide = URL::KNoUniStr;

		OP_DELETEA(g_tempuniurl);
		g_tempuniurl = new_tempuniurl;
		g_tempurl_len = TEMPBUFFER_SHRINK_LIMIT;
	}
}

OP_STATUS URL_Name::Set_Name(URL_Name_Components_st &url)
{
	// Important: ensure that a buffer exists even for a NULL url, as
	// the buffer is accessed to return the name even for NULL urls.
	//PrintfTofile("url_con.txt","Constructing %s\n", url);
	unsigned templen = 32;  // Arbitrary minimum size

	if (url.full_url)
		templen = op_strlen(url.full_url);

	RETURN_IF_ERROR(CheckBufferSize(templen+40));

	basic_components = url.basic_components;

	RETURN_IF_ERROR(path.Set(url.path));

	return query.Set(url.query);
}

URL_Name::~URL_Name()
{
#ifdef YNP_DEBUG_NAME
	if(InList())
		Out();
#endif
	//PrintfTofile("url_con.txt","Constructing %s\n", Name());

	if (last_user == this)
		last_user = NULL;

	if (uni_last_user == this)
		uni_last_user = NULL;
}

const char *URL_Name::Name(URL::URL_NameStringAttribute password_hide, URL_RelRep* rel_rep, int display_link_id) const
{ // No LogFunc

	// Return illegal url for display, this way the user can edit the url and retry.
	if (illegal_original_url.HasContent())
	{
		last_user = NULL;
		UTF16toUTF8Converter converter;

		unsigned orig_url_len = illegal_original_url.Length();
		if(OpStatus::IsError(converter.Construct()) ||
		   OpStatus::IsError(CheckBufferSize(4*(orig_url_len + 1)))) // 4 is max expansion in utf16->utf8 conversion.
			return NULL;

		int read = 0;
		int written = converter.Convert(illegal_original_url.CStr(), UNICODE_SIZE(orig_url_len + 1), g_tempurl, g_tempurl_len - 1, &read);
		if (written == -1)
			return NULL;

		g_tempurl[written] = '\0';
		return g_tempurl;
	}

	if(basic_components == NULL && !display_link_id && (!rel_rep || rel_rep->Name().IsEmpty()))
		return path.CStr();

	if(!g_tempurl || 
		(display_link_id == URL_NAME_LinkID_None &&
		last_user == this &&
		(URL::URL_NameStringAttribute) last_hide ==  password_hide &&
		last_rel_rep == rel_rep))
			return g_tempurl;

	BOOL remove_escapes = FALSE;
	
	last_user = NULL;

	switch(password_hide)
	{
	case URL::KName:
	case URL::KName_Username:
	case URL::KName_Username_Password_NOT_FOR_UI:
	case URL::KName_Username_Password_Hidden:
	case URL::KName_With_Fragment:
	case URL::KName_With_Fragment_Username:
	case URL::KName_With_Fragment_Username_Password_NOT_FOR_UI:
	case URL::KName_With_Fragment_Username_Password_Hidden:
		if(basic_components != NULL)
			remove_escapes = TRUE;
		break;
	}

	if (OpStatus::IsError(CheckBufferSize((basic_components != NULL ? basic_components->GetMaxOutputStringLength() : 0) + path.Length())))
		return NULL;

	g_tempurl[0] = '\0';

	if(display_link_id == URL_NAME_LinkID_Unique)
	{
		// Target is always long enough for this result.
		op_snprintf(g_tempurl, g_tempurl_len, "\\%p\\", this);
	}

	if(basic_components != NULL)
		basic_components->OutputString(password_hide, g_tempurl, g_tempurl_len, display_link_id);

	if(path.HasContent())
		op_strncat(g_tempurl, path.CStr(), g_tempurl_len-op_strlen(g_tempurl));

	if(remove_escapes && basic_components != NULL && basic_components->scheme_spec && basic_components->scheme_spec->used_urltype != URL_DATA)
	{
		if (OpStatus::IsError(CheckBufferSize((op_strlen(g_tempurl)+1)*3)))
			return NULL;

		UriUnescape::ReplaceChars(g_tempurl, UriUnescape::SafeUtf8);
	}

	if(query.HasContent())
	{
		size_t current_len = op_strlen(g_tempurl);
		if (OpStatus::IsError(CheckBufferSize((current_len + query.Length() + 1))))
			return NULL;
		op_strncat(g_tempurl, query.CStr(), g_tempurl_len-op_strlen(g_tempurl));
	}

	if (rel_rep && rel_rep->Name().HasContent())
	{
		size_t url_len = op_strlen(g_tempurl);
		if(OpStatus::IsSuccess(CheckBufferSize(url_len+ rel_rep->Name().Length()+1)) )
		{
			size_t buffer_len = g_tempurl_len-url_len;
			op_strncat(g_tempurl, "#",1);
			op_strncat(g_tempurl, rel_rep->Name().CStr(), buffer_len-1);
		}
	}

	if( display_link_id == URL_NAME_LinkID_None) // Link IDs are seldom used.
	{
		last_user = this;
		last_hide = (URL::URL_StringAttribute) password_hide;
		last_rel_rep = rel_rep;
	}

	return g_tempurl;
}

const uni_char *URL_Name::UniName(URL::URL_UniNameStringAttribute password_hide, unsigned short charsetID, URL_RelRep* rel_rep) const
{ // No LogFunc
	// Return illegal url for display, this way the user can edit the url and retry.
	if (illegal_original_url.HasContent())
		return illegal_original_url.CStr();

	if (g_tempuniurl==NULL || (uni_last_user == this &&
		(URL::URL_UniNameStringAttribute) uni_last_hide == password_hide &&
		uni_last_rel_rep == rel_rep))
		return g_tempuniurl;

	uni_last_user = NULL;

	UTF8toUTF16Converter converter;

	if(OpStatus::IsError(converter.Construct()))
		return NULL;

	int len2;
	int len;
	int read = 0;

	BOOL remove_escapes = FALSE;
	BOOL special_output = FALSE;

	g_tempuniurl[0] = '\0';

	if(basic_components != NULL)
	{
		if (OpStatus::IsError(CheckBufferSize(basic_components->GetMaxOutputStringLength() + 1)))
			return NULL;
		basic_components->OutputString(password_hide, g_tempuniurl, g_tempurl_len);
	}

	const OpString8 *path_component = &path;
#ifdef URL_FILE_SYMLINKS
	if (password_hide == URL::KUniName_For_Data && symbolic_link_target_path.HasContent())
		path_component = &symbolic_link_target_path;
#endif // URL_FILE_SYMLINKS

	switch(password_hide)
	{
	case URL::KUniName:
	case URL::KUniName_Username:
	case URL::KUniName_Username_Password_NOT_FOR_UI:
	case URL::KUniName_Username_Password_Hidden:
	case URL::KUniName_With_Fragment:
	case URL::KUniName_With_Fragment_Username:
	case URL::KUniName_With_Fragment_Username_Password_NOT_FOR_UI:
	case URL::KUniName_With_Fragment_Username_Password_Hidden:
	case URL::KUniName_For_Data:
		if(basic_components != NULL && 
			charsetID != 0 && 
			g_url_module.GetEnabledURLNameCharset(charsetID)) // 
		{
			special_output = TRUE;
			if(path_component->HasContent())
			{
				len2 = uni_strlen(g_tempuniurl);
				int len3 = path_component->Length();
				int len4=0;

				len = 0;
				if(len3 != 0) // either use the whole path or some of it.
				{
					last_user = NULL;

					if (OpStatus::IsError(CheckBufferSize(3*(len3+1))))
						return NULL;
					op_strlcpy(g_tempurl,path_component->CStr(), len3+1);
					UriUnescape::ReplaceChars(g_tempurl, UriUnescape::NonCtrlAndEsc);

					InputConverter *ch_converter = NULL;
					if(OpStatus::IsError(InputConverter::CreateCharConverterFromID(charsetID, &ch_converter)) ||
					   OpStatus::IsError(CheckBufferSize((len2+op_strlen(g_tempurl)+1)*3))) // Assume 3 utf-16 code points per char worst case.
						return NULL;
					int read =0;
					len = ch_converter->Convert(g_tempurl, op_strlen(g_tempurl),(char *) (g_tempuniurl+len2), UNICODE_SIZE(g_tempurl_len-len2-1), &read);

					OP_DELETE(ch_converter);
				}

				len4 = len2 + UNICODE_DOWNSIZE(len);
				g_tempuniurl[len4] = 0;
			}
			break;
		}

		remove_escapes = TRUE;
		// Fall through for all other (non-charset)
	default:
		if(path_component->HasContent())
		{
			len2 = uni_strlen(g_tempuniurl);
			len = path_component->Length();
			if (OpStatus::IsError(CheckBufferSize(len2+len+1))) // utf-8 -> utf-16 will never grow the string.
				return NULL;
			len = converter.Convert(path_component->CStr(), len, (char *) (g_tempuniurl + len2), UNICODE_SIZE(g_tempurl_len-len2-1), &read);
			if(len == -1)
				return NULL;

			len2 += UNICODE_DOWNSIZE(len);
			g_tempuniurl[len2] = 0;
		}
	}

	if(password_hide != URL::KUniName_For_Tel)
	{
		if(query.HasContent())
		{
			len2 = uni_strlen(g_tempuniurl);
			len = query.Length();
			if (OpStatus::IsError(CheckBufferSize(len2+len+1))) // utf-8 -> utf-16 will never grow the string.
				return NULL;
			len = converter.Convert(query.CStr(), len, (char *) (g_tempuniurl + len2), UNICODE_SIZE(g_tempurl_len-len2-1), &read);
			if(len == -1)
				return NULL;
	
			len2 += UNICODE_DOWNSIZE(len);
			g_tempuniurl[len2] = 0;
		}
	}

	if (rel_rep && rel_rep->Name().CStr())
	{
		len = rel_rep->Name().Length();
		len2 = uni_strlen(g_tempuniurl);

		if(OpStatus::IsError(CheckBufferSize(len+len2+2)))
			return NULL;

		g_tempuniurl[len2++] = '#';
		len = converter.Convert(rel_rep->Name().DataPtr(), len, (char *) (g_tempuniurl + len2), UNICODE_SIZE(g_tempurl_len-len2-1), &read);
		if(len == -1)
			return NULL;
		len2 += UNICODE_DOWNSIZE(len);
		g_tempuniurl[len2] = 0;
	}

	if(!special_output)
	{
		if (remove_escapes && basic_components != NULL &&  basic_components->scheme_spec && basic_components->scheme_spec->used_urltype != URL_DATA)
			UriUnescape::ReplaceChars(g_tempuniurl, UriUnescape::SafeUtf8);

		uni_last_user = this;
		uni_last_hide = password_hide;
		uni_last_rel_rep = rel_rep;
	}

	return g_tempuniurl;
}


OP_STATUS URL_Name::GetSuggestedFileName(OpString &target, BOOL only_extension/* = FALSE*/) const
{
	target.Empty();

	if (basic_components == (URL_Scheme_Authority_Components *) NULL || !basic_components->scheme_spec)
		return OpStatus::OK;

	URLType url_type = basic_components->scheme_spec->used_urltype;
	if(url_type == URL_JAVASCRIPT || url_type == URL_DATA || url_type == URL_OPERA || url_type == URL_UNKNOWN || url_type == URL_NULL_TYPE)
		return OpStatus::OK;

	if(path.IsEmpty())
		return OpStatus::OK;

	const char* pathstart = path.CStr();

	const char* pathend = op_strpbrk(pathstart, "\\\"");
	if (!pathend) pathend = pathstart + path.Length();

	const char* filenamestart = pathend;

	while (filenamestart > pathstart)
	{
		char c = *(filenamestart-1);
		
		if (c == '/')
			break;

		filenamestart--;

		c = *filenamestart;
		if (c == ';' || c == '*' || c == '|')
			pathend = filenamestart;
	}

	int len = pathend - filenamestart;
	if (len == 0)
		return OpStatus::OK;

#ifdef SYS_CAP_FILENAMES_IN_SYSTEM_CODEPAGE
	BOOL use_utf8 =	g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseUTF8Urls);

	if (!use_utf8)
	{
		OpString8 suggested;

		RETURN_IF_ERROR(suggested.Set(filenamestart, len));

		len = UriUnescape::ReplaceChars(suggested.CStr(), UriUnescape::NonCtrlAndEsc);

		const char* chs = NULL;

		if (basic_components->server_name)
		{
			CharsetDetector detector(basic_components->server_name->Name());
			detector.PeekAtBuffer(suggested.CStr(), len);
			chs = detector.GetDetectedCharset();
		}

		if (chs)
		{
			RETURN_IF_ERROR(SetFromEncoding(&target, chs, suggested.CStr(), len));
			len = target.Length();
		}
		else	// no server name to be used as hint or charset not determined, try converting using native codepage
		{
			uni_char *fname = (uni_char *) g_memory_manager->GetTempBufUni();
			
			int ret = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, suggested, -1, fname, g_memory_manager->GetTempBufUniLenUniChar());
			if (ret > 0)
			{
				len = ret-1;		// returned length includes terminating NUL
				RETURN_IF_ERROR(target.Set(fname, len));
			}
			else
				RETURN_IF_ERROR(target.Set(suggested, len));
		}
	}
	else
#endif //SYS_CAP_FILENAMES_IN_SYSTEM_CODEPAGE
	{
		RETURN_IF_ERROR(target.Set(filenamestart, len));
		len = UriUnescape::ReplaceChars(target.CStr(), UriUnescape::LocalfileAllUtf8);
	}

	while(--len >= 0) {
		uni_char c = target[len];
		if (c=='/' || c=='\\' || c==';' || c==':' ||c=='*' ||c=='\"' ||c=='|' || ( c=='.' && only_extension))
		{
			target.Delete(0,len+1);
			break;
		}
	}
	return OpStatus::OK;
}


uint32 URL_Name::GetAttribute(URL::URL_Uint32Attribute attr) const
{
	switch(attr)
	{
	case URL::KHaveServerName:
		return (basic_components != NULL && basic_components->server_name != NULL ? TRUE : FALSE);
	case URL::KInternal_AuthorityLength:
		return (basic_components != NULL ? basic_components->max_length : 0);
	case URL::KOverRideUserAgentId:
		return (basic_components != NULL && basic_components->server_name != NULL ? ((ServerName *) ((const ServerName *)basic_components->server_name))->GetOverRideUserAgent() : g_uaManager->GetUABaseId());
	case URL::KResolvedPort:
		return (basic_components != NULL ? 
					(basic_components->port ? basic_components->port : 
						(basic_components->scheme_spec ? basic_components->scheme_spec->default_port :0))
							: 0);
		/*
		{
			OpStringC8 pr = GetAttribute(URL::KProtocolName);
			
			if(pr.CompareI("ssl")    == 0)
				return 443;
			if (pr.CompareI("socks")  == 0)
				return 1080;
		}
		break;
		*/
	case URL::KRealType:
		if(basic_components != NULL && basic_components->scheme_spec)
			return basic_components->scheme_spec->real_urltype;

		return path.HasContent() ? URL_UNKNOWN : URL_NULL_TYPE;
	case URL::KServerPort:
		return (basic_components != NULL ? basic_components->port : 0);
	case URL::KType:
		if(basic_components != NULL && basic_components->scheme_spec)
			return basic_components->scheme_spec->used_urltype;

		return path.HasContent() ? URL_UNKNOWN : URL_NULL_TYPE;

	}
	return 0;
}

const OpStringC8 URL_Name::GetAttribute(URL::URL_StringAttribute attr, URL_RelRep* rel_rep) const
{
	OpStringC8 empty_ret;

	switch(attr)
	{
	case URL::KHostName:
		{
			OpStringC8 ret(basic_components != NULL && basic_components->server_name != NULL ? basic_components->server_name->Name() : empty_ret.CStr());
			return ret;
		}
	case URL::KPath:
		return path;
	case URL::KQuery:
		return query;
	case URL::KPassWord:
		if(basic_components != NULL)
			return basic_components->password;
		else
			return empty_ret;
	case URL::KUserName:
		if(basic_components != NULL)
			return basic_components->username;
		else
			return empty_ret;
	case URL::KProtocolName:
		{
			OpStringC8 ret(basic_components != NULL && basic_components->scheme_spec ? basic_components->scheme_spec->protocolname : NULL);
			return ret;
		}
	case URL::KHostNameAndPort_L:
	case URL::KPathAndQuery_L:
		OP_ASSERT(!"Use GetAttributeL for this attribute");

	}

	return empty_ret;
}


const OpStringC URL_Name::GetAttribute(URL::URL_UniStringAttribute attr, URL_RelRep* rel_rep) const
{
	OpStringC empty_ret;

	switch(attr)
	{
	case URL::KUniHostName:
		if(basic_components != NULL && basic_components->server_name != NULL)
		{ 
			OpStringC ret(basic_components->server_name->UniName());
			return ret;
		}
		break;
	case URL::KUniHostNameAndPort_L:
		OP_ASSERT(!"Use GetAttributeL for this attribute");
	}
	return empty_ret;
}

const OpStringC8 URL_Name::GetAttribute(URL::URL_NameStringAttribute attr, URL_RelRep* rel_rep) const
{
	switch(attr)
	{
	case URL::KName:
	case URL::KName_Escaped:
	case URL::KName_Username:
	case URL::KName_Username_Escaped:
	case URL::KName_Username_Password_NOT_FOR_UI:
	case URL::KName_Username_Password_Escaped_NOT_FOR_UI:
	case URL::KName_Username_Password_Hidden:
	case URL::KName_Username_Password_Hidden_Escaped:
		rel_rep = NULL; // We do not use fragmentidentifiers for these
	/*case URL::KName_With_Fragment:
	case URL::KName_With_Fragment_Escaped:
	case URL::KName_With_Fragment_Username:
	case URL::KName_With_Fragment_Username_Escaped:
	case URL::KName_With_Fragment_Username_Password_NOT_FOR_UI:
	case URL::KName_With_Fragment_Username_Password_Escaped_NOT_FOR_UI:
	case URL::KName_With_Fragment_Username_Password_Hidden:
	case URL::KName_With_Fragment_Username_Password_Hidden_Escaped:*/
	}

	OpStringC8 ret(Name(attr, rel_rep));
	return ret;
}


const OpStringC URL_Name::GetAttribute(URL::URL_UniNameStringAttribute attr, URL_RelRep* rel_rep) const
{
	switch(attr)
	{
	case URL::KUniPathAndQuery:
	case URL::KUniPathAndQuery_Escaped:
	case URL::KUniPath_Escaped:
	case URL::KUniPath:
	case URL::KUniQuery_Escaped:
	case URL::KUniPath_FollowSymlink:
	case URL::KUniPath_FollowSymlink_Escaped:
		{
			g_tempunipath.Empty();
			OP_STATUS op_err = GetAttribute(attr, 0, g_tempunipath);
			OpStatus::Ignore(op_err); // OOM: No way to report back. Callers should start using the GetAttributeL method
			return g_tempunipath;
		}
	case URL::KUniName:
	case URL::KUniName_Escaped:
	case URL::KUniName_Username:
	case URL::KUniName_Username_Escaped:
	case URL::KUniName_Username_Password_NOT_FOR_UI:
	case URL::KUniName_Username_Password_Escaped_NOT_FOR_UI:
	case URL::KUniName_Username_Password_Hidden:
	case URL::KUniName_Username_Password_Hidden_Escaped:
		rel_rep = NULL; // We do not use fragmentidentifiers for these
	/*case URL::KUniName_With_Fragment:
	case URL::KUniName_With_Fragment_Escaped:
	case URL::KUniName_With_Fragment_Username:
	case URL::KUniName_With_Fragment_Username_Escaped:
	case URL::KUniName_With_Fragment_Username_Password_NOT_FOR_UI:
	case URL::KUniName_With_Fragment_Username_Password_Escaped_NOT_FOR_UI:
	case URL::KUniName_With_Fragment_Username_Password_Hidden:
	case URL::KUniName_With_Fragment_Username_Password_Hidden_Escaped:*/
	}

	OpStringC ret(UniName(attr, 0, rel_rep));
	return ret;
}

OP_STATUS URL_Name::GetAttribute(URL::URL_StringAttribute attr, OpString8 &val, URL_RelRep* rel_rep) const
{
	switch(attr)
	{
	case URL::KPathAndQuery_L:
		return val.SetConcat(path,query);
	case URL::KHostNameAndPort_L:
		{
			val.Empty();

			URLType url_type = (basic_components != NULL && basic_components->scheme_spec ? basic_components->scheme_spec->used_urltype : URL_NULL_TYPE);
			switch(url_type)
			{
			case URL_HTTP :
			case URL_HTTPS :
			case URL_Gopher :
			case URL_WAIS :
			case URL_MAILTO :
			case URL_NEWS :
			case URL_SNEWS :
			case URL_TELNET :
			case URL_TN3270:
			case URL_WEBSOCKET:
			case URL_WEBSOCKET_SECURE:
				{
					const ServerName *server_name = (basic_components != NULL ? 
						(const ServerName *)basic_components->server_name : NULL);
					if(server_name && server_name->Name() && *server_name->Name())
					{
						char *tempbuf = (char *) g_memory_manager->GetTempBuf2k();

						if(basic_components->port)
						{
							op_snprintf(tempbuf, g_memory_manager->GetTempBuf2kLen(), ":%u", basic_components->port);
							tempbuf[g_memory_manager->GetTempBuf2kLen()-1] = '\0';
						}
						else
							tempbuf[0] = '\0';

						return val.SetConcat(basic_components->server_name->Name(), tempbuf);
					}
				}
			}
		}
		break;		
	default:
		return val.Set(GetAttribute(attr, rel_rep));
	}

	return OpStatus::OK;
}

OP_STATUS URL_Name::GetAttribute(URL::URL_UniStringAttribute attr, OpString &val, URL_RelRep* rel_rep) const
{
	val.Empty();
	switch(attr)
	{
	case URL::KUniHostNameAndPort_L:
		val.Empty();
		switch(basic_components != NULL && basic_components->scheme_spec ? basic_components->scheme_spec->used_urltype : URL_NULL_TYPE)
		{
		case URL_HTTP :
		case URL_HTTPS :
		case URL_Gopher :
		case URL_WAIS :
		case URL_MAILTO :
		case URL_NEWS :
		case URL_SNEWS :
		case URL_TELNET :
		case URL_TN3270:
		case URL_WEBSOCKET:
		case URL_WEBSOCKET_SECURE:
			{
				const ServerName *server_name = (basic_components != NULL ? 
														(const ServerName *) basic_components->server_name : NULL);
				if(server_name && server_name->UniName() && *server_name->UniName())
				{
					uni_char *tempbuf = (uni_char *) g_memory_manager->GetTempBuf2k();
					
					if(basic_components->port)
					{
						uni_snprintf(tempbuf, UNICODE_DOWNSIZE(g_memory_manager->GetTempBuf2kLen()), UNI_L(":%u"), basic_components->port);
						tempbuf[UNICODE_DOWNSIZE(g_memory_manager->GetTempBuf2kLen()-1)] = '\0';
					}
					else
						tempbuf[0] = '\0';
					
					return val.SetConcat(basic_components->server_name->UniName(), tempbuf);
				}
			}
		}

		break;		
	case URL::KUniNameFileExt_L:
		return GetSuggestedFileName(val, TRUE);
		break;
	case URL::KSuggestedFileName_L:
		return GetSuggestedFileName(val);
		break;
	default:
		return val.Set(GetAttribute(attr, rel_rep));
	}

	return OpStatus::OK;
}

OP_STATUS URL_Name::GetAttribute(URL::URL_UniNameStringAttribute attr, unsigned short charsetID, OpString &val, URL_RelRep* rel_rep) const
{
	val.Empty();
	switch(attr)
	{
	case URL::KUniPath_FollowSymlink:
	case URL::KUniPath_FollowSymlink_Escaped:
		{
			URL::URL_UniNameStringAttribute new_attr;
#ifdef URL_FILE_SYMLINKS
			if (symbolic_link_target_path.HasContent())
				new_attr = (attr == URL::KUniPath_FollowSymlink) ? URL::KUniPath_SymlinkTarget : URL::KUniPath_SymlinkTarget_Escaped;
			else
#endif // URL_FILE_SYMLINKS
				new_attr = (attr == URL::KUniPath_FollowSymlink) ? URL::KUniPath : URL::KUniPath_Escaped;
						
			return GetAttribute(new_attr, charsetID, val, rel_rep);
		}
	case URL::KUniPathAndQuery:
	case URL::KUniPathAndQuery_Escaped:
	case URL::KUniPath_Escaped:
	case URL::KUniPath:
#ifdef URL_FILE_SYMLINKS
	case URL::KUniPath_SymlinkTarget:
	case URL::KUniPath_SymlinkTarget_Escaped:
#endif // URL_FILE_SYMLINKS
		{
			const OpString8 *path_component = &path;
#ifdef URL_FILE_SYMLINKS
			if (attr == URL::KUniPath_SymlinkTarget || attr == URL::KUniPath_SymlinkTarget_Escaped)
				path_component = &symbolic_link_target_path;
#endif // URL_FILE_SYMLINKS

			if(path_component->HasContent())
			{
				RETURN_IF_ERROR(val.Set(*path_component));
				if(val.HasContent() && attr != URL::KUniPath_Escaped && attr != URL::KUniPathAndQuery_Escaped
#ifdef URL_FILE_SYMLINKS
					&& attr != URL::KUniPath_SymlinkTarget_Escaped
#endif // URL_FILE_SYMLINKS
					)
				{
					if(charsetID == 0)
						UriUnescape::ReplaceChars(val.DataPtr(), UriUnescape::SafeUtf8);

					int len = UriUnescape::ReplaceChars(val.DataPtr(), UriUnescape::NonCtrlAndEsc);

					if(charsetID)
					{
						OpString temp;
		
						temp.TakeOver(val);
						return SetFromEncoding(&val, g_charsetManager->GetCanonicalCharsetFromID(charsetID),temp.CStr(),len);
					}
				}
			}
			if(query.HasContent() && (attr == URL::KUniPathAndQuery || attr == URL::KUniPathAndQuery_Escaped))
				return val.Append(query);
		}
		return OpStatus::OK;
	case URL::KUniQuery_Escaped:
		return val.Set(query);
	case URL::KUniName:
	case URL::KUniName_Escaped:
	case URL::KUniName_Username:
	case URL::KUniName_Username_Escaped:
	case URL::KUniName_Username_Password_NOT_FOR_UI:
	case URL::KUniName_Username_Password_Escaped_NOT_FOR_UI:
	case URL::KUniName_Username_Password_Hidden:
	case URL::KUniName_Username_Password_Hidden_Escaped:
		rel_rep = NULL; // We do not use fragmentidentifiers for these
		// Fall through to default
	/*case URL::KUniName_With_Fragment:
	case URL::KUniName_With_Fragment_Escaped:
	case URL::KUniName_With_Fragment_Username:
	case URL::KUniName_With_Fragment_Username_Escaped:
	case URL::KUniName_With_Fragment_Username_Password_NOT_FOR_UI:
	case URL::KUniName_With_Fragment_Username_Password_Escaped_NOT_FOR_UI:
	case URL::KUniName_With_Fragment_Username_Password_Hidden:
	case URL::KUniName_With_Fragment_Username_Password_Hidden_Escaped:
	case URL::KUniName_For_Tel: */
	default:
		return val.Set(UniName(attr, charsetID, rel_rep));
	}
}


const void *URL_Name::GetAttribute(URL::URL_VoidPAttribute  attr) const
{
	if ((const URL_Scheme_Authority_Components*)basic_components)
	{
		switch(attr)
		{
		case URL::KProtocolScheme:
			return basic_components->scheme_spec;
		case URL::KServerName:
			return (const ServerName *)basic_components->server_name;
		}
	}

	return NULL;
}

OP_STATUS URL_Name::SetAttribute(URL::URL_UniNameStringAttribute attr, const OpStringC &string)
{
#ifdef URL_FILE_SYMLINKS
	switch (attr)
	{
	case URL::KUniPath_SymlinkTarget:
		return symbolic_link_target_path.Set(string);
	}
#endif // URL_FILE_SYMLINKS
	return OpStatus::OK;
}

/* Set Attribute of name is disabled since no current flags can be set in the name
void URL_Name::SetAttribute(URL::URL_Uint32Attribute attr, uint32 value)
{
}
*/

BOOL URL_Name::operator==(const URL_Name &other) const
{
	return
		basic_components->scheme_spec == other.basic_components->scheme_spec &&
		basic_components->server_name == other.basic_components->server_name &&
		basic_components->port        == other.basic_components->port        &&
		basic_components->username    == other.basic_components->username    &&
		basic_components->password    == other.basic_components->password    &&
		path                          == other.path                          &&
		query                         == other.query;
}
