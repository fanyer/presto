/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef _HTTP_REQ_2_H_
#define _HTTP_REQ_2_H_

#include "modules/upload/upload.h"

#ifdef ABSTRACT_CERTIFICATES
#include "modules/pi/network/OpCertificate.h"
#endif // ABSTRACT_CERTIFICATES

#include "modules/url/protocols/common.h"
#include "modules/url/protocols/http1.h"

class HTTP_Request : public ProtocolComm, public Upload_EncapsulatedElement
{
	friend class HTTP_1_1;
	friend class HTTP_Server_Manager;
	friend class URL_HTTP_LoadHandler;
	friend class URL_NameResolve_LoadHandler;
	friend class AuthElm;
	friend class Digest_AuthElm;
	friend class URL_External_Request_Handler;
	friend class Cookie_Item_Handler;
	private:
		HTTP_1_1 *http_conn;
		OpSmartPointerNoDelete<HTTP_Server_Manager> master;
		HeaderInfo_Pointer    header_info;
#ifdef _SSL_SUPPORT_
		HTTP_Request	*secondary_request;
#endif
		UINT m_tcp_connection_established_timout;

		int sendcount;
		int connectcount;
		struct reqinfo_st{
			UINT waiting:1;
			UINT sending_request:1;
			UINT sent_request:1;

			UINT header_loaded:1;

			UINT single_name_expansion:1;
			UINT proxy_namecomplete:1;

			UINT proxy_request:1;
			UINT proxy_no_cache:1;
			UINT priority:5;

			UINT use_range_start:1;
			UINT use_range_end:1;

#if defined(_SSL_SUPPORT_) || defined(_NATIVE_SSL_SUPPORT_) || defined(_CERTICOM_SSL_SUPPORT_)
			UINT secure:1;
#endif
			UINT read_secondary:1;
			UINT proxy_connect_open:1;

			UINT send_expect_100_continue:1;
			UINT send_100c_body:1;

			UINT force_waiting:1;
			UINT is_formsrequest:1;

			UINT data_with_headers:1;
			UINT send_close:1;
			UINT managed_request:1;
#ifdef _URL_EXTERNAL_LOAD_ENABLED_
			UINT ex_is_external:1;
			UINT upload_buffer_underrun:1;
#endif
			UINT retried_loading:1;
			UINT pipeline_used_by_previous:1;
#ifdef HTTP_AGGRESSIVE_PIPELINE_TIMEOUT
			UINT sent_pipelined:1;
#endif
#ifdef _SUPPORT_PROXY_NTLM_AUTH_
			UINT do_ntlm_authentication:1;
			UINT ntlm_updated_server:1;
			UINT ntlm_updated_proxy:1;
#endif

			UINT disable_automatic_refetch_on_error:1;
#if defined(WEBSERVER_SUPPORT) && defined(WEBSERVER_DIRECT_SOCKET_SUPPORT)
			UINT unite_request:1;
			UINT unite_admin_request:1;
#endif // WEBSERVER_SUPPORT

#if defined(_EMBEDDED_BYTEMOBILE) || defined(WEB_TURBO_MODE)
			UINT load_direct:1;
#endif // _EMBEDDED_BYTEMOBILE || WEB_TURBO_MODE
#ifdef WEB_TURBO_MODE
			UINT bypass_turbo_proxy:1;
			UINT first_turbo_request:1;
			UINT turbo_proxy_auth_retry:1;
#endif // WEB_TURBO_MODE
#ifdef _EMBEDDED_BYTEMOBILE
			UINT bypass_ebo_proxy:1;
#endif // _EMBEDDED_BYTEMOBILE
#ifdef URL_ALLOW_DISABLE_COMPRESS
			UINT disable_compress:1;
#endif	
#ifdef HTTP_CONTENT_USAGE_INDICATION
			UINT usage_indication:3;
#endif // HTTP_CONTENT_USAGE_INDICATION
			UINT open_extra_idle_connections:1; // If sending many requests to same server, open extra connections to opitmize.
			UINT loading_finished:1; // Loading finished has been called already, so the request is going to be destroyed and it's not safe to load it again.
			UINT disable_headers_reduction:1;
			UINT privacy_mode:1;
		}info;

		//int requeued;
		
		HTTP_Method method;
		OpString8	method_string;
		HTTP_request_st *request;

#ifdef WEB_TURBO_MODE
		unsigned long turbo_auth_id;
#endif // WEB_TURBO_MODE
#ifdef _ENABLE_AUTHENTICATE
		unsigned long auth_id;
		unsigned long proxy_auth_id;

#ifdef HTTP_DIGEST_AUTH
		HTTP_Request_digest_data auth_digest, proxy_digest;
#endif
#ifdef _SUPPORT_PROXY_NTLM_AUTH_
		AuthElm		*proxy_ntlm_auth_element;
		AuthElm		*server_negotiate_auth_element;
#endif
#endif
		
		OpStringS8	referer;
		
		OpFileLength range_start;
		OpFileLength range_end;
		
		OpStringS8	data;
		OpStringS8	content_type;

		OpStringS8	if_modified_since;
#ifdef _HTTP_USE_IFUNMODIFIED
		OpStringS8	if_unmodified_since;
#endif
#ifdef _HTTP_USE_IFMATCH
		OpStringS8	if_match;
#endif
		OpStringS8	if_none_match;

		AutoDeleteHead	TE_decoding;
		time_t	loading_started;

#ifdef _URL_EXTERNAL_LOAD_ENABLED_
		OpString8	ex_connection_header;
		OpStringS8	ex_cookies;
		Head ex_standard_headers;
#endif

		OpFileLength previous_load_length;
		OpFileLength previous_load_difference;

#if defined(_EMBEDDED_BYTEMOBILE) || defined(SCOPE_HTTP_LOGGER) || defined(WEB_TURBO_MODE)
		int m_request_number;
#endif  // _EMBEDDED_BYTEMOBILE || SCOPE_HTTP_LOGGER || WEB_TURBO_MODE
		OpSocketAddressNetType use_nettype; // Which nettypes are permitted
		MH_PARAM_1	msg_id;

#ifdef OPERA_PERFORMANCE
public:
	OpProbeTimestamp time_request_created;
#endif // OPERA_PERFORMANCE

#ifdef SCOPE_RESOURCE_MANAGER
	BOOL	has_sent_response_finished;
#endif //SCOPE_RESOURCE_MANAGER
private:
	void			InternalInit(
		HTTP_Method meth, 
		HTTP_request_st *req,
		UINT tcp_connection_established_timout,
		BOOL open_extra_idle_connections
#if defined(_SSL_SUPPORT_) || defined(_NATIVE_SSL_SUPPORT_) || defined(_CERTICOM_SSL_SUPPORT_)
		, BOOL sec
#endif
		);
	void			InternalDestruct();


	public:

		HTTP_Request(
			MessageHandler* msg_handler, 
			HTTP_Method meth, 
			HTTP_request_st *request,
			UINT tcp_connection_established_timout,
			BOOL open_extra_idle_connections
#if defined(_SSL_SUPPORT_) || defined(_NATIVE_SSL_SUPPORT_)
			, BOOL sec
#endif
			);
		//OP_STATUS Construct();
		void ConstructL(
#ifdef IMODE_EXTENSIONS
		BOOL use_ua_utn = FALSE
#endif
			);

		virtual ~HTTP_Request();

		OP_STATUS SetUpMaster();

		void PrepareHeadersL();
		char* ComposeRequestL(unsigned& len);
#if defined(_SSL_SUPPORT_) && (!defined(_EXTERNAL_SSL_SUPPORT_)  || defined(_USE_HTTPS_PROXY))
		char * ComposeConnectRequest(unsigned& len);
#endif

		unsigned short Port() const {return master->Port();};
		ServerName *HostName() {return master->HostName();};
		virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
		void EnableSingleNameExpansion(BOOL val){ info.single_name_expansion = val;};

		void Set_NetType(OpSocketAddressNetType val){use_nettype = val;} // Which nettypes are permitted
		OpSocketAddressNetType Get_NetType() const {return use_nettype;} // Which nettypes are permitted

		virtual CommState	Load();
		virtual CommState	ConnectionEstablished();
		virtual void		SendData(char *buf, unsigned blen); // buf is always freed by the handling object
		virtual void		RequestMoreData();
		virtual unsigned	ReadData(char* buf, unsigned blen);
		virtual void		EndLoading();
		virtual void		SetProgressInformation(ProgressState progress_level, 
											 unsigned long progress_info1, 
											 const void *progress_info2);

		virtual void Stop();
		void Clear();

		HeaderInfo* GetHeader();
		OP_STATUS SetExternalHeaderInfo(HeaderInfo *);

		const char *GetPath(){return request->request.CStr();};
		const char *GetAbsolutePath(){return request->path;};

		void      SetSendingRequest(BOOL val) { info.sending_request = val; };
		BOOL      GetSendingRequest() const { return info.sending_request; };
		void      SetSentRequest(BOOL val) { info.sent_request = val; };
		BOOL      GetSentRequest() const { return info.sent_request; };
		void      SetWaiting(BOOL val) { info.waiting = val; };
		BOOL      GetWaiting() const { return info.waiting; };
		void      SetForceWaiting(BOOL val) { info.force_waiting = val; };
		BOOL      GetForceWaiting() const { return info.force_waiting; };
		void      SetIsFormsRequest(BOOL val) { info.is_formsrequest = val; };
		BOOL      GetIsFormsRequest() const { return info.is_formsrequest; };
		void      SetPrivacyMode(BOOL val) { info.privacy_mode = val; };
		BOOL      GetPrivacyMode() const { return info.privacy_mode; };
		BOOL      GetSendCount() const { return sendcount; };
		void      IncSendCount() { ++sendcount; };
#if defined(_SSL_SUPPORT_) || defined(_NATIVE_SSL_SUPPORT_) || defined(_CERTICOM_SSL_SUPPORT_)
		BOOL      GetIsSecure() const { return info.secure; };
#endif // defined(_SSL_SUPPORT_) || defined(_NATIVE_SSL_SUPPORT_) || defined(_CERTICOM_SSL_SUPPORT_)
#ifdef WEB_TURBO_MODE
		void      SetIsFirstTurboRequest(BOOL val) { info.first_turbo_request = val; };
		void      SetIsTurboProxyAuthRetry(BOOL val) { info.turbo_proxy_auth_retry = val; };
		void      SetDisableHeadersReduction(BOOL val) { info.disable_headers_reduction = val; };
		void      SetBypassTurboProxy(BOOL val) { info.bypass_turbo_proxy = val; };
		BOOL      GetBypassTurboProxy() const { return info.bypass_turbo_proxy; };
#endif // WEB_TURBO_MODE

		OP_STATUS	SetMethod(HTTP_Method val, const OpStringC8 &mth_str) {
									method = val; 
									if(method == HTTP_METHOD_String && mth_str.HasContent()) 
										return method_string.Set(mth_str);
									return OpStatus::OK;
								};
		HTTP_Method   GetMethod() const { return method; };
		const char*	GetMethodString() const;
		const char*	GetSchemeString() const;
		void      SetHeaderLoaded(BOOL val) { info.header_loaded = val; };
		BOOL      GetHeaderLoaded() const { return info.header_loaded; };
		void      SetProxyRequest(BOOL val) { info.proxy_request = val; };
		public:
		BOOL      GetProxyRequest() const { return info.proxy_request; };

		void      SetProxyNoCache(BOOL val) { info.proxy_no_cache = val; };
		BOOL      GetProxyNoCache() const { return info.proxy_no_cache; };
		void      SetPriority(UINT val) { OP_ASSERT(val<=URL_HIGHEST_PRIORITY); info.priority = val; };
		UINT      GetPriority() const { return info.priority; };
#ifdef HTTP_CONTENT_USAGE_INDICATION
		void      SetUsageIndication(HTTP_ContentUsageIndication val) { info.usage_indication = val; }
		HTTP_ContentUsageIndication GetUsageIndication() { return (HTTP_ContentUsageIndication)info.usage_indication; }
#endif

public:
		const char *Origin_HostName() const;
		const uni_char *Origin_HostUniName() const { return request->origin_host->GetUniName(); }
		unsigned short Origin_HostPort() const;
		const char* GetDate() const;

		void LoadingFinished(BOOL send_msg= TRUE);

		void	SetCookieL(OpStringC8 str,
			int version,
			int max_version
			);
		void		SetAuthorization(const OpStringC8 &auth);
		unsigned long	GetAuthorizationId(){return auth_id;};
		void		SetAuthorizationId(unsigned long aid){auth_id=aid;};
#ifdef WEB_TURBO_MODE
		void		SetTurboAuthId(unsigned long aid){turbo_auth_id=aid;};
#endif // WEB_TURBO_MODE
		void		SetProxyAuthorization(const OpStringC8 &auth);
		unsigned long		GetProxyAuthorizationId();
		void		SetProxyAuthorizationId(unsigned long aid);
		void		SetRefererL(OpStringC8 ref);
		OpStringC8  GetReferer() const { return referer; }
		void		SetDataL(OpStringC8 dat, BOOL with_headers);
#ifdef HAS_SET_HTTP_DATA
		void		SetData(Upload_Base* dat);
#endif

		void		SetRequestContentTypeL(OpStringC8 ct);
		void		SetIfModifiedSinceL(OpStringC8 mod);
		OpStringC8	GetIfModifiedSince() { return if_modified_since; }
		void		SetIfRangeL(OpStringC8 ent);	
#ifdef _HTTP_USE_IFMATCH
		void		SetIfMatchL(OpStringC8 ent);	
#endif
		void		SetIfNoneMatchL(OpStringC8 ent);
		virtual OP_STATUS SetCallbacks(MessageObject* master, MessageObject* parent);

		void SetRangeStart(OpFileLength pos){range_start = pos;info.use_range_start = (pos != FILE_LENGTH_NONE);};
		void SetRangeEnd(OpFileLength pos){range_end = pos;info.use_range_end = (pos != FILE_LENGTH_NONE);};
		OpFileLength GetRangeStart() { return range_start; }
		OpFileLength GetRangeEnd() { return range_end; }
		void SetReadSecondaryData(){info.read_secondary=TRUE;};
#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
		BOOL MoveToNextProxyCandidate();
#endif
		void SetMasterHTTP_Continue(BOOL val){master->SetHTTP_Continue(val);};

#ifdef URL_ALLOW_DISABLE_COMPRESS
		void SetDisableCompress(BOOL disable) { info.disable_compress = (disable ? TRUE : FALSE); }
#endif
#ifdef _HTTP_COMPRESS
		BOOL AddTransferDecodingL(HTTP_compression); // must be added in same sequence as in transferencoding field
#endif
#ifdef HTTP_DIGEST_AUTH
		OP_STATUS Check_Digest_Authinfo(HTTP_Authinfo_Status &ret_status, ParameterList *header, BOOL proxy, BOOL finished);
		void ProcessEntityBody(const unsigned char *, unsigned);
		void ClearAuthentication();
#endif
#ifdef _URL_EXTERNAL_LOAD_ENABLED_
		void		AddCookiesL(OpStringC8 str);
		void		AddConnectionParameterL(OpStringC8 str);
		void SetExternal() { info.ex_is_external = TRUE; }
		BOOL IsExternal() { return info.ex_is_external; }
		OP_STATUS AppendUploadData(const void *buf, int bufsize);
#endif
#if defined(_EMBEDDED_BYTEMOBILE) || defined(SCOPE_HTTP_LOGGER) || defined(WEB_TURBO_MODE)
		void SetRequestNumber(int request_number) { m_request_number = request_number; }
		int GetRequestNumber() { return m_request_number; }
#endif  // _EMBEDDED_BYTEMOBILE || SCOPE_HTTP_LOGGER || WEB_TURBO_MODE

#if defined(_EMBEDDED_BYTEMOBILE) || defined(WEB_TURBO_MODE)
		void SetLoadDirect(){info.load_direct = TRUE;}
		BOOL GetLoadDirect(){return info.load_direct;}
#endif // _EMBEDDED_BYTEMOBILE || WEB_TURBO_MODE
#ifdef _EMBEDDED_BYTEMOBILE
		void SetBypassEBOProxy(){info.bypass_ebo_proxy = TRUE;}
		BOOL GetBypassEBOProxy(){return info.bypass_ebo_proxy;}
#endif // _EMBEDDED_BYTEMOBILE

		void SetExternalHeaders(Header_List *headers);
		OP_STATUS CopyExternalHeadersToUrl(URL &url);

		void SetSendAcceptEncodingL(BOOL flag);

#ifdef TCP_PAUSE_DOWNLOAD_EXTENSION
	/* Pause a HTTP request by stop doing recv() on the socket,
		   loading can be resumed without a second GET request. */
		void PauseLoading();
	/* Continues loading a HTTP request which has been paused with PauseLoading(). */
		OP_STATUS ContinueLoading();
#endif // TCP_PAUSE_DOWNLOAD_EXTENSION
#ifdef ABSTRACT_CERTIFICATES
		OpCertificate* ExtractCertificate() { return conn ? conn->ExtractCertificate() : NULL; }
#endif // ABSTRACT_CERTIFICATES

#if (defined EXTERNAL_ACCEPTLANGUAGE || defined EXTERNAL_HTTP_HEADERS)
	void SetContextId(URL_CONTEXT_ID id) { context_id = id; }
	URL_CONTEXT_ID context_id;
#endif // EXTERNAL_ACCEPTLANGUAGE

	void SetMsgID(MH_PARAM_1 id){msg_id = id;}
	MH_PARAM_1 GetMsgId() const {return msg_id;}

#ifdef URL_NOTIFY_FILE_DOWNLOAD_SOCKET
	void SetIsFileDownload(BOOL value) { if( conn ) conn->SetIsFileDownload(value); }
#endif

#ifdef SCOPE_RESOURCE_MANAGER

	/**
	 * This struct is used to carry information about HTTP requests and responses
	 * when using ProgressState::REPORT_LOAD_STATUS.
	 */
	struct LoadStatusData_Raw
	{
		LoadStatusData_Raw(HTTP_Request* req, const char* buf, size_t len)
			: req(req), buf(buf), len(len) { OP_ASSERT(req); }

		// The HTTP_Request that generated the (possibly) attached raw data.
		HTTP_Request* req;
				
		// Points to the raw data, or NULL if not available.
		const char* buf;

		// The size of 'buf'.
		size_t len;
	}; // LoadStatusData_Raw

	/**
	 * This struct is used to carry information when using ProgressState::REPORT_LOAD_STATUS
	 * with OpScopeResourceListener::LS_COMPOSE_REQUEST.
	 */
	struct LoadStatusData_Compose
	{
		LoadStatusData_Compose(HTTP_Request* req, int orig_id)
			: req(req), orig_id(orig_id) { OP_ASSERT(req); }

		// The HTTP_Request we are composing a request for.
		HTTP_Request* req;

		// The previous request ID for this request, or zero if there isn't any.
		int orig_id;
	}; // LoadStatusData_Retry

#endif // SCOPE_RESOURCE_MANAGER
};

#endif // _HTTP_REQ_2_H_
