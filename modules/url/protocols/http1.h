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


#ifndef _HTTP_1_1_H_
#define _HTTP_1_1_H_

#include "modules/hardcore/timer/optimer.h"

#define HTTP_AGGRESSIVE_PIPELINE_TIMEOUT

// http1.h

// HTTP 1.1 protocol and administration
#include "modules/url/protocols/http_met.h"
#include "modules/url/protocols/connman.h"
#include "modules/url/protocols/next_protocol.h"
#include "modules/url/protocols/pcomm.h"
#include "modules/formats/argsplit.h"
#include "modules/formats/hdsplit.h"
#include "modules/util/adt/opvector.h"
#include "modules/url/protocols/common.h"
#include "modules/probetools/src/probetimer.h"
#include "modules/url/protocols/http_requests_handler.h"

#ifdef _EMBEDDED_BYTEMOBILE
#include "modules/url/protocols/ebo/bm_information_provider.h"
#endif

#ifdef WEB_TURBO_MODE
#include "modules/upload/upload.h"
#endif // WEB_TURBO_MODE

#include "modules/olddebug/timing.h"

class SSL_Hash;
class HTTP_1_1;

class OpHTTP_IDVector : private OpGenericVector
{
public:

	/**
	 * Creates a vector, with a specified allocation step size.
	 */
	OpHTTP_IDVector(UINT32 stepsize = 10) : OpGenericVector(stepsize) {}

	/**
	 * Clear vector, returning the vector to an empty state
	 */
	void Clear() { OpGenericVector::Clear(); }

	/**
	 * Sets the allocation step size
	 */
	void SetAllocationStepSize(UINT32 step) { OpGenericVector::SetAllocationStepSize(step); }

	/**
	 * Makes this a duplicate of vec.
	 */
	OP_STATUS DuplicateOf(const OpHTTP_IDVector& vec) { return OpGenericVector::DuplicateOf(vec); }

	/**
	 * Replace the item at index with new item
	 */
	OP_STATUS Replace(UINT32 idx, MH_PARAM_1 item) { return OpGenericVector::Replace(idx, (void*) item); }

	/**
	 * Insert and add an item of type T at index idx. The index must be inside
	 * the current vector, or added to the end. This does not replace, the vector will grow.
	 */
	OP_STATUS Insert(UINT32 idx, MH_PARAM_1 item) { return OpGenericVector::Insert(idx, (void*) item); }

	/**
	 * Adds the item to the end of the vector.
	 */
	OP_STATUS Add(MH_PARAM_1 item) { return OpGenericVector::Add((void*) item); }

	/**
	 * Removes item if found in vector
	 */
	OP_STATUS RemoveByItem(MH_PARAM_1 item) { return OpGenericVector::RemoveByItem((void*) item); }

	/**
	 * Removes count items starting at index idx and returns the pointer to the first item.
	 */
	MH_PARAM_1 Remove(UINT32 idx, UINT32 count = 1) { return (MH_PARAM_1)(OpGenericVector::Remove(idx, count)); }

	/**
	 * Finds the index of a given item
	 */
	INT32 Find(INT32 item) const { return OpGenericVector::Find((void*)(INTPTR) item); }

	/**
	 * Returns the item at index idx.
	 */
	MH_PARAM_1 Get(UINT32 idx) const { return (MH_PARAM_1)(OpGenericVector::Get(idx)); }

	/**
	 * Returns the number of items in the vector.
	 */
	UINT32 GetCount() const { return OpGenericVector::GetCount(); }
};

struct HeaderInfo : public OpReferenceCounter {
	HeaderList headers, trailingheaders;
	OpStringS8 response_text;
    int   response;
	OpFileLength content_length;
#ifdef WEB_TURBO_MODE
	UINT32 turbo_transferred_bytes;
	UINT32 turbo_orig_transferred_bytes;
#endif // WEB_TURBO_MODE
	time_t loading_started;

	// The method of the request this header is in response to
	HTTP_Method original_method;

	struct headerinfo{
		BYTE http1_1:1;
		BYTE received_close:1;
		BYTE has_trailer:1;
		BYTE received_content_length:1;
		BYTE http_10_or_more:1;
#ifdef USE_SPDY
		BYTE spdy:1;
#endif // USE_SPDY
		//BYTE content_waiting:1;
	} info;

	HeaderInfo(HTTP_Method meth) : original_method(meth) { 
				loading_started = 0;
				response = HTTP_NO_RESPONSE;
				content_length =0;
#ifdef WEB_TURBO_MODE
				turbo_transferred_bytes = 0;
				turbo_orig_transferred_bytes = 0;
#endif // WEB_TURBO_MODE
				info.http1_1 = 
					info.has_trailer = 
					info.received_close = 
					info.received_content_length = 
#ifdef USE_SPDY
					info.spdy = 
#endif // USE_SPDY
					/*info.content_waiting = */ FALSE;
				info.http_10_or_more = TRUE;
	};
	void Clear(){
				response_text.Empty();
				loading_started = 0;
				response = HTTP_NO_RESPONSE;
				content_length =0;
#ifdef WEB_TURBO_MODE
				turbo_transferred_bytes = 0;
				turbo_orig_transferred_bytes = 0;
#endif // WEB_TURBO_MODE
				info.http1_1 = 
					info.has_trailer = 
					info.received_close = 
					info.received_content_length = 
					/*info.content_waiting = */ FALSE;
	}
	virtual ~HeaderInfo() {};
};

typedef OpSmartPointerWithDelete<HeaderInfo> HeaderInfo_Pointer;



struct HTTP_request_st : public Base_request_st
{
#ifdef _EMBEDDED_BYTEMOBILE
	BOOL predicted;
	int predicted_depth;
#endif
#ifdef URL_SIGNAL_PROTOCOL_TUNNEL
	BOOL is_tunnel;
#endif // URL_SIGNAL_PROTOCOL_TUNNEL
#ifdef WEBSERVER_SUPPORT
	BOOL unite_connection;
	BOOL unite_admin_connection;
#endif // WEBSERVER_SUPPORT
#ifdef WEB_TURBO_MODE
	BOOL use_turbo;
	BOOL use_proxy_passthrough;
#endif // WEB_TURBO_MODE

	const char *userid;
	const char *password;

	const char *path; // pointer into request to path component;
	// If proxy request this holds the entire URL except in the
	// case of HTTPS requests, then servername and protocol are not included;

	HTTP_request_st(){
#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
		current_connect_host = NULL;
#endif
		connect_host = origin_host = NULL; 
		connect_port = origin_port = 0;

		proxy = NO_PROXY;
#ifdef _EMBEDDED_BYTEMOBILE
		predicted = FALSE;
		predicted_depth = 0;
#endif
#ifdef URL_SIGNAL_PROTOCOL_TUNNEL
		is_tunnel = FALSE;
#endif // URL_SIGNAL_PROTOCOL_TUNNEL
#ifdef WEBSERVER_SUPPORT
		unite_connection = FALSE;
		unite_admin_connection = FALSE;
#endif // WEBSERVER_SUPPORT
#ifdef WEB_TURBO_MODE
		use_turbo = FALSE;
		use_proxy_passthrough = FALSE;
#endif // WEB_TURBO_MODE

		userid = password = NULL;
		path = NULL;
	};
	~HTTP_request_st(){
#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
		connect_list.Clear();
		current_connect_host = NULL;
#endif
		userid = password = NULL; 
		request.Wipe();
	};
#ifdef WEB_TURBO_MODE
	BOOL IsTurboProxy();
#endif // WEB_TURBO_MODE
};

class HTTP_Request;

#ifdef _HTTP_COMPRESS
enum HTTP_compression { 
	HTTP_Unknown_encoding=HTTP_General_Tag_Unknown,
	HTTP_Identity = HTTP_General_Tag_Identity,
	HTTP_Chunked =	HTTP_General_Tag_Chunked,
	HTTP_Deflate =	HTTP_General_Tag_Deflate, 
	HTTP_Compress = HTTP_General_Tag_Compress,
	HTTP_Gzip =		HTTP_General_Tag_Gzip
};
#endif

class HTTP_Connection;

class HTTP_Server_Manager : public Connection_Manager_Element
{
	friend class HTTP_Manager;
	private:
		unsigned int http_1_1:1;
		unsigned int http_continue:1;
		unsigned int http_chunking:1;
		unsigned int http_1_0_keep_alive:1;
		unsigned int http_1_0_used_connection_header:1;
		unsigned int http_1_1_pipelined:1;
//#ifndef HTTP_AGGRESSIVE_PIPELINE_TIMEOUT
		unsigned int http_1_1_used_keepalive:1;
//#endif
		unsigned int http_no_pipeline:1;
		unsigned int tested_http_1_1_pipelinablity:1;
		unsigned int http_protcol_determined:1;
		unsigned int http_always_use_continue:1;
		unsigned int http_trust_content_length:1;
		int pipeline_problem_count;
#ifdef WEB_TURBO_MODE
		unsigned int turbo_enabled:1;
		OpString8 m_turbo_host;
#endif // WEB_TURBO_MODE
		Head delayed_request_list;
		NextProtocol nextProtocolNegotiated;
	
		CommState SetupHttpConnection(HTTP_Request *request, SComm *comm, HTTP_Connection *&conn, NextProtocol protocol);
	public:
#if defined(_SSL_SUPPORT_) || defined(_NATIVE_SSL_SUPPORT_) || defined(_CERTICOM_SSL_SUPPORT_)
		HTTP_Server_Manager(ServerName *name, unsigned short port_num, BOOL sec);
#else
		HTTP_Server_Manager(ServerName *name, unsigned short port_num);
#endif
		virtual ~HTTP_Server_Manager();

		int GetMaxServerConnections(HTTP_Request *req);
		BOOL BypassDocumentConnections(HTTP_Request *req, unsigned max_server_connections);

		/*  Attempt to create an idle connection.
		 *
		 *  The idle connection is created if:
		 *  - The number  other idle connections to server is not bigger than number_of_max_existing_idle_connections.
		 *  - Number of connections to server is less than maximum
		 *  - Total number of connections is less than total maximum - 5
		 *
		 * Note that number_of_max_existing_idle_connections also counts connections not yet connected
		 * (and thus not yet an idle connection).
		 *
		 * @param request request from which proxy information for connection is retrieved
		 * @param number_of_max_existing_idle_connections maximum number of existing idle connections
		 */
		void AttemptCreateExtraIdleConnection(HTTP_Request *request, int number_of_max_existing_idle_connections = 0);
		CommState CreateNewConnection(HTTP_Request *req, class HTTP_Connection *&conn);
		CommState AddRequest(HTTP_Request *);
		void RemoveRequest(HTTP_Request *);
		void ForciblyMoveRequest(HTTP_1_1 *connection);
		virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
		void IncrementPipelineProblem();
		void SetHTTP_1_1(BOOL val){http_1_1 = val;};
		void SetHTTP_1_0_KeepAlive(BOOL val){http_1_0_keep_alive = val;};
		void SetHTTP_1_0_UsedConnectionHeader(BOOL val){http_1_0_used_connection_header = val;};
		void SetHTTP_1_1_Pipelined(BOOL val){http_1_1_pipelined = val;}
		void SetHTTP_No_Pipeline(BOOL val){http_no_pipeline = val;}
		void SetHTTP_Trust_ContentLength(BOOL val){http_trust_content_length = val;}
		void SetTestedHTTP_1_1_Pipeline(BOOL val){tested_http_1_1_pipelinablity = val;}
		void SetHTTP_Continue(BOOL val){http_continue = val;};
		void SetAlwaysUseHTTP_Continue(BOOL val);
		void SetHTTP_ProtocolDetermined(BOOL val){http_protcol_determined= val;};
//#ifndef HTTP_AGGRESSIVE_PIPELINE_TIMEOUT
		void SetUsedKeepAlive(){http_1_1_used_keepalive = TRUE;}
//#endif
		BOOL GetHTTP_1_1() const{return http_1_1;}
		BOOL GetHTTP_Continue() const{return http_continue;}
		BOOL GetHTTP_No_Pipeline() const{return http_no_pipeline;}
		BOOL GetHTTP_1_0_UsedConnectionHeader() const{return http_1_0_used_connection_header;}
		BOOL GetAlwaysUseHTTP_Continue() const{return http_always_use_continue;}
		BOOL GetHTTP_Trust_ContentLength() const{return http_trust_content_length;}
		BOOL GetHTTP_ProtocolDetermined() const{return http_protcol_determined;}
//#ifndef HTTP_AGGRESSIVE_PIPELINE_TIMEOUT
		BOOL GetUsedKeepAlive() const{return http_1_1_used_keepalive;}
//#endif
#ifdef WEB_TURBO_MODE
		void SetTurboEnabled() { turbo_enabled = TRUE; }
		BOOL GetTurboEnabled() const { return turbo_enabled; }
		void SetTurboHostL(const char* host) { m_turbo_host.SetL(host); }
		const char* GetTurboHost() const { return m_turbo_host.HasContent() ? m_turbo_host.CStr() : NULL; }
#endif // WEB_TURBO_MODE

		// Forces all other connections with this server to stop sending requests, 
		// and terminate after the last pending request has been handled
		void ForceOtherConnectionsToClose(HTTP_1_1 *caller); 
		void RequestHandled();

		virtual BOOL Preserve() { return HostName()->IsHostResolved() || pipeline_problem_count > 0; }

		int GetNumberOfIdleConnections(BOOL include_not_yet_connected_connections);
		void NextProtocolNegotiated(HttpRequestsHandler *connection, NextProtocol next_protocol);
};

class HTTP_Transfer_Decoding;
class Upload_Base;

class HTTP_Connection : public Connection_Element
{
#ifdef USE_SPDY
	BOOL isSpdyConnection;
#endif // USE_SPDY
public:
	HttpRequestsHandler *conn;
	HTTP_Connection(HttpRequestsHandler *nconn, BOOL isSpdy);
	virtual ~HTTP_Connection();

	virtual BOOL Idle();
	virtual BOOL AcceptNewRequests();
	virtual BOOL HasPriorityRequest(UINT priority);
	virtual void RestartRequests();
	virtual unsigned int Id();
	virtual BOOL SafeToDelete();
	virtual BOOL HasId(unsigned int sid);
	virtual void SetNoNewRequests();
#ifdef USE_SPDY
	void SetIsSpdyConnection(BOOL isSpdy);
	BOOL IsSpdyConnection() { return isSpdyConnection; }
#endif // USE_SPDY
};

class HTTP_Request_Head;

class HTTP_Request_List : private Link
{
		friend class HTTP_Request_Head;
	public:
		~HTTP_Request_List() { if (InList()) Out(); }
		HTTP_Request_List* Suc() const { return static_cast<HTTP_Request_List*>(Link::Suc()); }
	    HTTP_Request_List* Pred() const { return static_cast<HTTP_Request_List*>(Link::Pred()); }
	    void Out();
	    void Into(HTTP_Request_Head* list);
	    void Precede(HTTP_Request_List* l);
	    void Follow(HTTP_Request_List* l);

	    void IntoStart(HTTP_Request_Head* list); 

		HTTP_Request *request;
};

class HTTP_Request_Head : private AutoDeleteHead
{
		friend class HTTP_Request_List;
	public:
		HTTP_Request_Head() : AutoDeleteHead(), m_count(0) {}
		HTTP_Request_List* First() const { return static_cast<HTTP_Request_List*>(AutoDeleteHead::First()); }
	    HTTP_Request_List* Last() const { return static_cast<HTTP_Request_List*>(AutoDeleteHead::Last()); }
	    BOOL Empty() const { return AutoDeleteHead::Empty(); }
	    UINT Cardinal() const { return m_count; }
	private:
		UINT m_count;
};

#ifdef OPERA_PERFORMANCE
class Http_Request_Log_Point: public Link
{
	const int connection_number;
	const int request_number;
	OpString8 request_string;
public:
	OpProbeTimestamp time_request_created;
	OpProbeTimestamp time_sent;
	OpProbeTimestamp time_header_loaded;
	OpProbeTimestamp time_completed;
	OpFileLength size;
	int http_code;
	int priority;
	Http_Request_Log_Point(int a_connection_number, int a_request_number, BOOL secure, const char *method, const char *server, const char *path, OpProbeTimestamp &time_request_created, int priority);
	void SetSize(OpFileLength a_size) { size = a_size; }
	void SetHttpCode(int code) { http_code = code; }
	void SetFinished() { time_completed.timestamp_now(); }
	void SetHeaderLoaded() { time_header_loaded.timestamp_now(); }
	int GetRequestNumber() { return request_number; }
	int GetConnectionNumber() { return connection_number; }
	const char *GetName() { return request_string.CStr(); }
};

class Http_Connection_Log_Point: public Link
{
	int connection_number;
	OpString8 server_name;
public:
	OpProbeTimestamp time_connection_created;
	OpProbeTimestamp time_connected;
	OpProbeTimestamp time_closed;
	BOOL connected, closed, pipelined;
	Http_Connection_Log_Point(int a_connection_number, const char *server) { server_name.Set(server); connection_number = a_connection_number; time_connection_created.timestamp_now(); connected = FALSE; closed = FALSE; pipelined = FALSE; }
	void SetConnected() { time_connected.timestamp_now(); connected = TRUE; }
	void SetClosed() { time_closed.timestamp_now();  closed = TRUE; }
	void SetPipelined(BOOL value) { pipelined = value; }
	BOOL GetPipelined() { return pipelined; }
	int GetConnectionNumber() { return connection_number; }
	const char *GetServerName() { return server_name.CStr(); }
};

class Message_Processing_Log_Point: public Link
{
public:
	OpProbeTimestamp time_start_processing;
	double time;
	OpString8 message_string;
	Message_Processing_Log_Point(const char *message, OpProbeTimestamp &time_start_processing, double &time);
};

class Message_Delay_Log_Point: public Link
{
public:
	OpProbeTimestamp time_start_processing;
	double time;
	OpString8 message_string;
	OpString8 old_message_string;
	Message_Delay_Log_Point(const char *message, const char *old_message, OpProbeTimestamp &time_start_processing, double &time);
};
#endif // OPERA_PERFORMANCE

class HTTP_Manager : public Connection_Manager
{
#ifdef OPERA_PERFORMANCE
	Head http_request_log;
	Head http_connect_log;
	BOOL freeze_log;
public:
	virtual ~HTTP_Manager();
	void OnConnectionCreated(int connection_number, const char *server_name);
	void OnConnectionConnected(int connection_number);
	void OnConnectionClosed(int connection_number);
	void OnPipelineDetermined(int connection_number, BOOL support_pipeline_fully);
	void OnRequestSent(int connection_number, int id, BOOL secure, const char* method, const char* server, const char* path, OpProbeTimestamp &time_request_created, int priority);
	void OnHeaderLoaded(int id, int http_code);
	void OnResponseFinished(int id, OpFileLength size, OpProbeTimestamp &request_created);
	void Reset();
	void Freeze();
	void WriteReport(URL &url, OpProbeTimestamp &start_time);
#endif // OPERA_PERFORMANCE
public:
#if defined(_SSL_SUPPORT_) || defined(_NATIVE_SSL_SUPPORT_) || defined(_CERTICOM_SSL_SUPPORT_)
		void RemoveURLReferences();
		HTTP_Server_Manager *FindServer(ServerName *name, unsigned short port_num, BOOL sec, BOOL create = FALSE);
#else
		HTTP_Server_Manager *FindServer(ServerName *name, unsigned short port_num, BOOL create = FALSE);
#endif
#ifdef _OPERA_DEBUG_DOC_
		void generateDebugDocListing(URL *url);
#endif
};

class HTTP_1_1 : public  HttpRequestsHandler
#ifdef HTTP_CONNECTION_TIMERS
, public OpTimerListener
#endif
{
	friend class HTTP_Request;
	friend class URL_Manager;
	friend class HTTP_Server_Manager;
	friend class HTTP_Manager;

	protected:
		HTTP_Request *request, *sending_request;
		HTTP_Request_Head request_list;

	private:
#ifdef HTTP_CONNECTION_TIMERS
		OpTimer* m_response_timer;
		OpTimer* m_idle_timer;
		UINT m_response_timer_is_started : 1; 
		UINT m_idle_timer_is_started : 1; 
#endif
#ifdef URL_TURBO_MODE_HEADER_REDUCTION
		Header_List* m_common_headers;
		HeaderList* m_common_response_headers;
#endif // URL_TURBO_MODE_HEADER_REDUCTION

		struct http11_info_st
		{
			UINT connection_active : 1;
			UINT handling_header : 1;
			UINT continue_allowed:1;
			UINT is_uploading:1;
			UINT use_chunking:1;
			UINT waiting_for_chunk_header:1;
			UINT expect_trailer:1;
			UINT trailer_mode:1;
			UINT host_is_1_0:1;
			UINT http_1_0_keep_alive:1;
			UINT http_1_1_pipelined:1;
			UINT http_protocol_determined:1;
			UINT first_request:1;
			UINT connection_established : 1;
			UINT connection_failed : 1;
			UINT tested_http_1_1_pipelinablity:1;
			UINT activated_100_continue_fallback:1;
			UINT read_source:1;
			UINT handling_end_of_connection:1;
#ifdef _SSL_USE_SMARTCARD_
			UINT smart_card_authenticated:1;
#endif
#ifdef _EMBEDDED_BYTEMOBILE
			UINT ebo_authenticated:1;
			UINT ebo_optimized:1;
			UINT predicted:1;
			UINT pending_reqs_to_new_ebo_conn:1;
#endif // _EMBEDDED_BYTEMOBILE
#ifdef WEB_TURBO_MODE
			UINT turbo_proxy_auth_retry:1;
			UINT turbo_unhandled_response:1;
#endif // WEB_TURBO_MODE
			UINT restarting_requests:1;
			UINT request_list_updated:1;
		} info;

		int prev_response;
		HeaderInfo_Pointer	header_info;
		HeaderInfo_Pointer	selfheader_info;

		char		*header_str;
		OpData		content_buf;
		OpFileLength content_length, content_loaded;

		OpFileLength chunk_size, chunk_loaded;
		int			http_10_ka_max;
		int			request_count;

#ifdef _EMBEDDED_BYTEMOBILE
		BMInformationProvider bm_information_provider;
		OpString8 ebo_oc_req;
#endif // _EMBEDDED_BYTEMOBILE
#ifdef OPERA_PERFORMANCE
		int connection_number;
#endif // OPERA_PERFORMANCE

		time_t		last_active;
		
#ifdef HTTP_BENCHMARK
		LONGLONG	last_finished;
#endif
		OpHTTP_IDVector		pipelined_items;

		void      SetHeaderInfoL();
		BOOL      IsLegalHeader(const char *tmp_buf,OpFileLength cnt_len); // TRUE if legal, FALSE if not, handled internally if FALSE
		void      CleanHeader();

		BOOL      LoadHeaderL();

		void      SetHandlingHeader(BOOL val) { info.handling_header  = val; };
		BOOL      GetHandlingHeader() const { return info.handling_header; };

#if defined(_EMBEDDED_BYTEMOBILE) || defined(WEB_TURBO_MODE)
		BOOL		HandleOutOfOrderResponse(int seq_no);
#endif // _EMBEDDED_BYTEMOBILE || WEB_TURBO_MODE
#ifdef WEB_TURBO_MODE
		void		HandleBypassResponse(int error_code);
#endif // WEB_TURBO_MODE

		void		TestAndSetPipeline();

	public:
		const char *Origin_HostName() const;
		unsigned short Origin_HostPort() const;

	private:
		enum ChunkTerminatorFlags
		{
			CTF_CRLFCRLF = 1,
			CTF_LFCRLF = 2,
			CTF_LFLF = 4,
			CTF_CRLF = 8,
			CTF_LFCR = 16,
			CTF_CR = 32,
			CTF_LF = 64,
		};
		size_t FindChunkTerminator(OpData &data, unsigned terminatorFlags, size_t &longestTerminatorLength);
		OpFileLength ReadChunkedData(char *buf, OpFileLength blen);
		OpFileLength ProcessChunkedData(char *buf, OpFileLength blen, BOOL &more, BOOL &request_finished, BOOL &incorrect_format);
		void	 HandleEndOfConnection(MH_PARAM_2 par2=0);
		void InternalInit(ServerName* _host,  unsigned short _port);
		void InternalDestruct();
		void SignalPipelineReload();
		void OptimizeRequestOrder();


	protected:

		unsigned short protocol_def_port;
#if defined(HAS_SET_HTTP_DATA) 
		BOOL MoreRequest();
#endif
		void ComposeRequest();
		void    Clear();

		void MoveToNextRequest();
		BOOL MoveRequestsToNewConnection(HTTP_Request_List *item, BOOL forced=FALSE);

#ifdef HTTP_CONNECTION_TIMERS
		void StartResponseTimer();
		void StartIdleTimer();
		void StopResponseTimer();
		void StopIdleTimer();
#endif // HTTP_CONNECTION_TIMERS

	public:

		HTTP_1_1(MessageHandler* msg_handler, ServerName_Pointer hostname,	unsigned short portnumber, BOOL privacy_mode = FALSE);
		virtual void ConstructL();
		virtual ~HTTP_1_1();
		virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

		virtual CommState Load();
		virtual BOOL AcceptNewRequests();
		virtual BOOL HasPriorityRequest(UINT priority);
		virtual BOOL HasRequests();
		virtual unsigned int GetRequestCount() const;
		virtual unsigned int GetUnsentRequestCount() const;
		virtual BOOL AddRequest(HTTP_Request *, BOOL force_first = FALSE);
#ifdef URL_TURBO_MODE_HEADER_REDUCTION
		void ReduceHeadersL(Header_List& hdrList);
		void ExpandResponseHeadersL(HeaderList& hdrList);
#endif // URL_TURBO_MODE_HEADER_REDUCTION

		virtual void RemoveRequest(HTTP_Request *);
		void SetHTTP_1_1_flags(BOOL, BOOL, BOOL, BOOL, BOOL, BOOL, BOOL);
		HeaderInfo*   GetHeader() {return header_info;};
		virtual CommState ConnectionEstablished();
		virtual void		RequestMoreData();
		virtual unsigned int	ReadData(char* buf, unsigned blen);
		virtual void		ProcessReceivedData();
		virtual void		EndLoading();
		virtual void		Stop();

		virtual void RestartRequests();

		const char*   GetDate() const;

		virtual BOOL Idle();
		virtual BOOL IsActiveConnection() const;
		virtual BOOL IsNeededConnection() const;
		virtual HTTP_Request *MoveLastRequestToANewConnection();
		void OnLoadingFinished();
		virtual BOOL SafeToDelete() const;
		virtual void SetProgressInformation(ProgressState progress_level, 
											 unsigned long progress_info1, 
											 const void *progress_info2);

		time_t LastActive(){ return last_active;};
#ifdef _EMBEDDED_BYTEMOBILE
		void SetEboOptimized(){info.ebo_optimized = TRUE;}
		BOOL GetEboOptimized(){return info.ebo_optimized;}
		void SetEBOOCReq(const OpStringC8 &oc_req) { ebo_oc_req.Set(oc_req); }
		OpString8 &GetEBOOCReq() { return ebo_oc_req; }
		BMInformationProvider &GetBMInformationProvider() { return bm_information_provider; }
#endif //_EMBEDDED_BYTEMOBILE
#ifdef OPERA_PERFORMANCE
		int GetConnectionNumber() { return connection_number; }
#endif // OPERA_PERFORMANCE
#ifdef HTTP_CONNECTION_TIMERS
		virtual void OnTimeOut(OpTimer* timer);
#ifdef TCP_PAUSE_DOWNLOAD_EXTENSION
		virtual void        PauseLoading();
		virtual OP_STATUS   ContinueLoading();
#endif // TCP_PAUSE_DOWNLOAD_EXTENSION
#endif
};

/// Class able to parse a Range header, supporting "bytes=X-Y" syntax
/// The class validate the input
class HTTPRange
{
private:
	/// Beginning of range; FILE_LENGTH_NONE means no start (e.g. "bytes=-3" ==> last 3 bytes)
	OpFileLength range_start;
	/// End of range; FILE_LENGTH_NONE means no end (e.g. "bytes=3-" ==> from the third byte till the end)
	OpFileLength range_end;

	/// Skips all the whitespaces
	void SkipWhiteSpaces(const char *&ptr) { while (*ptr==' ') ptr++; }
	/// Skips all the whitespaces
	void SkipWhiteSpaces(char *&ptr) { while (*ptr==' ') ptr++; }

public:
	/// Default constructor. The object will not be valid until Parse() is called
	HTTPRange() { range_start = range_end = FILE_LENGTH_NONE; }
	
	/** Parse the string retrieving the range
		@param range String with the range
		@param assume_bytes if FALSE, the string (asfetr stripping spaces) needs to start with "bytes", else it will simply be
		       assumed that it is a byte range (though it can still start with "bytes")
		@param max_length if != FILE_LENGTH_NONE, it will represent the maximum value set for the end of the range, and it 
		       will also resolve the ranges, so that, for example, on a 500 bytes file, "bytes=-3" will become range 497-499
	**/
	OP_STATUS Parse(const char *range, BOOL assume_bytes, OpFileLength max_length = FILE_LENGTH_NONE);

	/// Return the beginning of the range; FILE_LENGTH_NONE means no start (e.g. "bytes=-3" ==> last 3 bytes)
	OpFileLength GetStart() { return range_start; }
	/// Return the end of the range; FILE_LENGTH_NONE means  no end (e.g. "bytes=3-" ==> from the third byte till the end)
	OpFileLength GetEnd() { return range_end; }

	/// Simplify some computation related to range_start and range_end, considering FILE_LENGTH_NONE as if it was 0.
	/// The caller still need to understand the difference between 0 (first byte) and FILE_LENGTH_NONE (no range)
	static OpFileLength ConvertFileLengthNoneToZero(OpFileLength value) { return (value==FILE_LENGTH_NONE) ? 0 : value; }
};

//typedef HTTP_1_1 HTTP;

#endif /* _HTTP_ */
