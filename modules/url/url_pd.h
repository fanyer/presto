/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/


#ifndef _URL_PD_H_
#define _URL_PD_H_

#include "modules/url/url_sn.h"
#include "modules/probetools/src/probetimer.h"
#include "modules/util/adt/oplisteners.h"

// URL Protocol Data

class MultiPart;
class Header_List;
class URL_Rep;
class OpCertificate;

class URL_ProtocolData 
{
public:
	virtual ~URL_ProtocolData(){};

	virtual uint32 GetAttribute(URL::URL_Uint32Attribute attr) const{return 0;};
	virtual const OpStringC8 GetAttribute(URL::URL_StringAttribute attr) const{OpStringC8 ret; return ret;};
	virtual const OpStringC GetAttribute(URL::URL_UniStringAttribute attr) const{OpStringC ret; return ret;};
	virtual OP_STATUS		GetAttribute(URL::URL_StringAttribute attr, OpString8 &val) const{return val.Set(GetAttribute(attr));};
	virtual OP_STATUS		GetAttribute(URL::URL_UniStringAttribute attr, OpString &val) const{return val.Set(GetAttribute(attr));};
	virtual const void *	GetAttribute(URL::URL_VoidPAttribute  attr, const void *param) const{return NULL;}
	virtual URL				GetAttribute(URL::URL_URLAttribute  attr){return URL();}
	virtual URL				GetAttribute(URL::URL_URLAttribute  attr, URL_Rep *url){return URL();}
	
	virtual void SetAttribute(URL::URL_Uint32Attribute attr, uint32 value){}
	virtual OP_STATUS SetAttribute(URL::URL_StringAttribute attr, const OpStringC8 &string){return OpStatus::OK;}
	virtual OP_STATUS SetAttribute(URL::URL_UniStringAttribute attr, const OpStringC &string){return OpStatus::OK;}
	virtual OP_STATUS SetAttribute(URL::URL_VoidPAttribute  attr, const void *param){return OpStatus::OK;}
	virtual OP_STATUS SetAttribute(URL::URL_URLAttribute  attr, const URL &param){return OpStatus::OK;}
};

class URL_HTTP_ProtocolData : public URL_ProtocolData
{
#ifdef ABSTRACT_CERTIFICATES	
	private:
		OpCertificate* m_certificate;
#endif
	public:
		struct flags_st{
#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
			UINT using_apc_proxy:1;
#endif
			UINT http_method:4;
			UINT http_data_new:1;
			UINT http_data_with_header:1;
			UINT http10_or_more:1;
			UINT auth_has_credetentials:1;
			UINT auth_credetentials_used:1;
			UINT auth_status:2;
			UINT proxy_auth_status:2;   // 20/08/97  YNP
			UINT connect_auth:1;
			UINT only_if_modified:1;
			UINT proxy_no_cache:1;
			UINT must_revalidate:1;
			UINT used_ua_id:3;
			UINT priority:5;
			UINT header_loaded_sent:1;
			UINT redirect_blocked:1;
			UINT always_check_never_expired_queryURLs:1;
			UINT enable_accept_encoding:1;
			/** 0: Not checking; 1: Checking; 2: use GET; 3: Use POST */
			UINT checking_redirect:2;
#ifdef HTTP_CONTENT_USAGE_INDICATION
			UINT usage_indication:3;
#endif
#ifdef CORS_SUPPORT
			UINT cors_redirects:3;
#endif // CORS_SUPPORT
			/** Set if an only_if_modified reload was requested and that
			    could be fulfilled by adding conditional (If-*) headers. */
			UINT added_conditional_headers:1;
		} flags;

		struct senddata_st {

			OpString		proxyname;
#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
			Head			proxy_connect_list;
#endif

			OpStringS8		http_content_type;
			OpStringS8		http_data;
#ifdef HAS_SET_HTTP_DATA
			Upload_Base *upload_data;
#endif
#ifdef URL_ENABLE_HTTP_RANGE_SPEC
			OpFileLength	range_start;
			OpFileLength	range_end;
#endif
			URL				referer_url;
			short			redirect_count;
			Header_List*	external_headers;
			OpString8		username;
			OpString8		password;
#ifdef WEB_TURBO_MODE
			BOOL			use_proxy_passthrough;
#endif // WEB_TURBO_MODE
		} sendinfo;

		// Persistent information
		struct keepinfo_st
		{
			OpStringS8		load_date;
			time_t			expires;
			unsigned long	age;
		    OpStringS8		last_modified;
			OpStringS8		encoding;
			OpStringS8		frame_options;
			OpStringS8		entitytag;
#ifdef OPERA_PERFORMANCE
			OpProbeTimestamp time_request_created;
#endif // OPERA_PERFORMANCE
		} keepinfo;

		// Last request specific information
		struct recvdata_st {
			URL				moved_to_url;
			OpStringS8		moved_to_url_name;
			OpStringS8		location;
			OpStringS8		content_location;
			OpStringS8		content_language;
			OpStringS8		response_text;
			OpStringS8      refresh_url;
			short			refresh_int;
			unsigned short	response;

			OpStringS		suggested_filename; // 18/01/99 YNP
			//HeaderList*		cookie_headers;
			HeaderList*		all_headers; // 2001-09-03 RL
#ifdef _SECURE_INFO_SUPPORT
			URL_InUse		secure_session_info;
#endif

			AutoDeleteHead	link_relations; // sequence of HTTP_Link_Relations
		} recvinfo;

		authdata_st *authinfo;

private:
	void InternalInit();
	void InternalDestruct();



public:
	URL_HTTP_ProtocolData();
	virtual ~URL_HTTP_ProtocolData();

	URL MovedToUrl(URL_Rep *url);
	
	BOOL CheckAuthData();
	
	void ClearHTTPData();
	
#ifdef _OPERA_DEBUG_DOC_
	unsigned long	GetMemUsed();
#endif

	virtual uint32 GetAttribute(URL::URL_Uint32Attribute attr) const;
	virtual const OpStringC8 GetAttribute(URL::URL_StringAttribute attr) const;
	virtual const OpStringC GetAttribute(URL::URL_UniStringAttribute attr) const;
	virtual OP_STATUS		GetAttribute(URL::URL_StringAttribute attr, OpString8 &val) const;
	virtual OP_STATUS		GetAttribute(URL::URL_UniStringAttribute attr, OpString &val) const;
	virtual const void *GetAttribute(URL::URL_VoidPAttribute  attr, const void *param) const;

	virtual URL GetAttribute(URL::URL_URLAttribute  attr, URL_Rep *url);
	
	virtual void SetAttribute(URL::URL_Uint32Attribute attr, uint32 value);
	virtual OP_STATUS SetAttribute(URL::URL_StringAttribute attr, const OpStringC8 &string);
	virtual OP_STATUS SetAttribute(URL::URL_UniStringAttribute attr, const OpStringC &string);
	virtual OP_STATUS SetAttribute(URL::URL_VoidPAttribute  attr, const void *param);
	virtual OP_STATUS SetAttribute(URL::URL_URLAttribute  attr, const URL &param);
};

class URL_FTP_ProtocolData : public URL_ProtocolData 
{
public:
	OpStringS8	MDTM_date;

public:
	//virtual uint32 GetAttribute(URL::URL_Uint32Attribute attr) const;
	virtual const OpStringC8 GetAttribute(URL::URL_StringAttribute attr) const;
	//virtual const OpStringC GetAttribute(URL::URL_UniStringAttribute attr) const;
	//virtual void			GetAttributeL(URL::URL_StringAttribute attr, OpString8 &val) const;
	//virtual void			GetAttributeL(URL::URL_UniStringAttribute attr, OpString &val) const;
	//virtual const void *GetAttribute(URL::URL_VoidPAttribute  attr, const void *param) const;
	
	//virtual void SetAttribute(URL::URL_Uint32Attribute attr, uint32 value);
	virtual OP_STATUS SetAttribute(URL::URL_StringAttribute attr, const OpStringC8 &string);
	///virtual void SetAttributeL(URL::URL_UniStringAttribute attr, const OpStringC &string);
	//virtual void SetAttributeL(URL::URL_VoidPAttribute  attr, const void *param);
};


class URL_Mailto_ProtocolData : public URL_ProtocolData 
{
public:
#ifdef HAS_SET_HTTP_DATA
	Upload_Base *upload_data;
#endif
 
  public:
  	URL_Mailto_ProtocolData();
  	virtual ~URL_Mailto_ProtocolData();

	//virtual uint32 GetAttribute(URL::URL_Uint32Attribute attr) const;
	//virtual const OpStringC8 GetAttribute(URL::URL_StringAttribute attr) const;
	//virtual const OpStringC GetAttribute(URL::URL_UniStringAttribute attr) const;
	//virtual void			GetAttributeL(URL::URL_StringAttribute attr, OpString8 &val) const;
	//virtual void			GetAttributeL(URL::URL_UniStringAttribute attr, OpString &val) const;
#ifdef HAS_SET_HTTP_DATA
	virtual const void *GetAttribute(URL::URL_VoidPAttribute  attr, const void *param) const;
#endif
	
	//virtual void SetAttribute(URL::URL_Uint32Attribute attr, uint32 value);
	//virtual void SetAttributeL(URL::URL_StringAttribute attr, const OpStringC8 &string);
	//virtual void SetAttributeL(URL::URL_UniStringAttribute attr, const OpStringC &string);
	//virtual void SetAttributeL(URL::URL_VoidPAttribute  attr, const void *param);
};

#ifdef _MIME_SUPPORT_
#include "modules/mime/attach_listener.h"

class URL_MIME_ProtocolData  : public URL_ProtocolData 
{
public:
	URL		 content_location_alias;
	URL		 content_base_location_alias;
	OpString body_comment_string;
#if defined(NEED_URL_MIME_DECODE_LISTENERS)
	OpListeners<MIMEDecodeListener> m_mimedecode_listeners;
#endif

public:

	URL_MIME_ProtocolData(){};
	// Contruction/destruction by default code

#ifdef NEED_URL_MIME_DECODE_LISTENERS
	virtual uint32 GetAttribute(URL::URL_Uint32Attribute attr) const;
#endif
	//virtual const OpStringC8 GetAttribute(URL_StringAttribute attr) const;
	virtual const OpStringC GetAttribute(URL::URL_UniStringAttribute attr) const;
	//virtual void			GetAttributeL(URL::URL_StringAttribute attr, OpString8 &val) const;
	//virtual void			GetAttributeL(URL::URL_UniStringAttribute attr, OpString &val) const;
	//virtual const void *GetAttribute(URL::URL_VoidPAttribute  attr, void *param) const;
	virtual URL GetAttribute(URL::URL_URLAttribute  attr);
	
	//virtual void SetAttribute(URL::URL_Uint32Attribute attr, uint32 value);
	//virtual void SetAttributeL(URL::URL_StringAttribute attr, const OpStringC8 &string);
	virtual OP_STATUS SetAttribute(URL::URL_UniStringAttribute attr, const OpStringC &string);
	virtual OP_STATUS SetAttribute(URL::URL_URLAttribute  attr, const URL &param);
#if defined(NEED_URL_MIME_DECODE_LISTENERS) 
	virtual OP_STATUS SetAttribute(URL::URL_VoidPAttribute  attr, const void *param);
#endif
};
#endif

#endif // !_URL_PD_H_
