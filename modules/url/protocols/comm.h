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

#ifndef _URL2_COMM_
#define _URL2_COMM_

#include "modules/pi/network/OpHostResolver.h"
#include "modules/pi/network/OpSocket.h"
#include "modules/pi/network/OpSocketAddress.h"
class CommCleaner;

#include "modules/url/protocols/scomm.h"
#include "modules/util/objfactory.h"
#include "modules/url/protocols/commelm.h"

#ifdef ABSTRACT_CERTIFICATES
#include "modules/pi/network/OpCertificate.h"
#endif // ABSTRACT_CERTIFICATES

#include "modules/hardcore/timer/optimer.h"

#define COMM_WAIT_STATUS_LOAD		0x01
#define COMM_WAIT_STATUS_DELETE		0x10
#define COMM_WAIT_STATUS_IS_DELETED	0x20

class ServerName;

class Comm : public SComm, public OpHostResolverListener, public OpSocketListener, public OpTimerListener
{
protected:
	friend class URL_Manager;
	friend class Window;
	friend class HTTP_Server_Manager;
	friend class CommCleaner;
	friend class SocketAndBuffer;

	class Comm_strings
		: public Link
	{
	public:
		char*				string;
		unsigned int		len;	
		BOOL				buffer_sent;
#ifdef _EMBEDDED_BYTEMOBILE
		unsigned			total_len;
#endif // _EMBEDDED_BYTEMOBILE

	public:
		Comm_strings(char *buf, unsigned blen) {
			string = buf; len = blen; buffer_sent = FALSE;
#ifdef _EMBEDDED_BYTEMOBILE
			total_len = blen;
#endif // _EMBEDDED_BYTEMOBILE
		}
		virtual ~Comm_strings();
	};

	ServerName_Pointer	m_Host; // host
	unsigned short		m_Port;	// port
	OpSocketAddressNetType use_nettype; // Which nettypes are permitted
	UINT 				m_max_tcp_connection_established_timeout;
	OpTimer				m_connection_established_timer;
	OpTimer				m_happy_eyeballs_timer;

	struct comminfo_st {
		UINT			connected:1;
		UINT			closed:1;
		UINT			only_connect:1;
		UINT			do_not_reconnect:1;
		UINT			pending_close:1;
		UINT			sending_in_progress:1;
		UINT			name_lookup_only:1;
#ifdef HAS_SET_HTTP_DATA
		UINT			is_uploading:1;
#endif
#ifdef SOCKS_SUPPORT
		UINT			use_socks:1;
#endif
		UINT			is_real_local_host:1;
		//UINT			read_data:1;
#ifdef _EMBEDDED_BYTEMOBILE
		UINT			zlib_transceive:1;
#endif //_EMBEDDED_BYTEMOBILE

#if defined(_EXTERNAL_SSL_SUPPORT_)
		UINT			is_secure:1;
#endif
		UINT			is_resolving_host:1;
		UINT			is_managed_connection:1;
#if defined(WEBSERVER_SUPPORT) && defined(WEBSERVER_DIRECT_SOCKET_SUPPORT)
		UINT			is_direct_embedded_webserver_connection:1;
#endif		
		UINT			resolve_is_from_initload:1; // prevent race cond during InitLoad
#ifdef WEBSOCKETS_SUPPORT
		UINT			is_full_duplex:1;
#endif // WEBSOCKETS_SUPPORT
	} info;

	int					LastError;

	int					m_ConnectAttempts;		//connect_attempts

	ProgressState		m_RequestMsgMode;						// request_msg_mode
	ProgressState		m_ConnectMsgMode;						// connect_msg_mode
	ProgressState		m_AllDoneMsgMode;						// All Done msg_mode
	ProgressState		m_AllDone_requestMsgMode;				// All Done request mode msg_mode

	time_t				last_waiting_for_connectionmsg;
	OpSocketAddress*	m_SocketAddress;
#ifdef OPSOCKET_GETLOCALSOCKETADDR
	OpSocketAddress*	m_local_socket_address;
#endif // #OPSOCKET_GETLOCALSOCKETADDR
	OpSocket*			m_Socket;

	OpSocketAddress*          m_alt_socket_address;
	OpSocket*                 m_alt_socket;
	
	Comm_strings*		current_buffer;
	AutoDeleteHead		buffers;


#ifdef CORESOCKETACCOUNTING
    int m_InstanceNumber;
#endif

protected:
	
	Comm(MessageHandler* msg_handler, ServerName* hostname, unsigned short portnumber, BOOL connect_only = FALSE);
	OP_STATUS Construct();

	void InternalInit(
			ServerName * _host,
			unsigned short _port,
			BOOL connect_only
			);
	void InternalDestruct();

public:

	static Comm* Create(MessageHandler* msg_handler, ServerName* hostname, unsigned short portnumber, BOOL connect_only = FALSE
#ifdef _EXTERNAL_SSL_SUPPORT_		
		, BOOL secure = FALSE
		, BOOL proxy_request = FALSE
#endif		
	);

	virtual				~Comm();

	ServerName*			HostName() { return m_Host; }
	unsigned short		Port() const { return m_Port; }

	/** Is this a specially/externally managed connection that does not count agains the server/total connection limits? */
	void				SetManagedConnection(){if(!m_Socket) info.is_managed_connection = TRUE;}

	void Set_NetType(OpSocketAddressNetType val){use_nettype = val;} // Which nettypes are permitted

	virtual void		HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
	virtual OP_STATUS	SetCallbacks(MessageObject* master, MessageObject* parent);
	virtual CommState	Load();
	virtual void 		SetMaxTCPConnectionEstablishedTimeout(UINT timeout);

	CommState			Load_Stage2();
	virtual void		SendData(char *buf, unsigned blen); // buf is always freed by the handling object
	virtual unsigned	ReadData(char* buf, unsigned blen);

	virtual BOOL		Connected() const;
	virtual BOOL		Closed();
	virtual void		Stop();				// Cut connection/reset connection

	int					GetLastError(){return LastError;};
	//int					GetCommErrorMsg(int wsa_err, const uni_char *wsa_func=NULL);
	int					GetCommErrorMsg(OpHostResolver::Error aError, const uni_char *wsa_func);
	int					GetCommErrorMsg(OpSocket::Error aSocketErr, const uni_char *wsa_func);

	CommState			SetSocket();
	virtual CommState	Connect();

	void				Cleanup();
	void				CloseSocket();
#ifdef SOCKET_SUPPORTS_TIMER
	void				SetSocketTimeOutInterval(unsigned int seconds, BOOL usedefault=FALSE) { m_Socket->SetTimeOutInterval(seconds,usedefault); }
#endif

	CommState			InitLoad();

	CommState			LookUpName(ServerName *name);

protected:

	static void			CommRemoveDeletedComm();
	static int			TryLoadBlkWaitingComm();
	static int			TryLoadWaitingComm(ServerName *);

	void				AddWaitingComm(BYTE status);
	void				RemoveWaitingComm();
	virtual void		RemoveDeletedComm();
	virtual void		MarkAsDeleted();
	static int			CountWaitingComm(BYTE flag);
	static void			CleanWaitingComm_List();

	virtual OP_STATUS	CreateSocket(OpSocket** aSocket, BOOL secure);
	CommState			SendDataToConnection();
	BOOL				MoreRequest();
	void				ClearBuffer();

	void				SetIsResolvingHost(BOOL flag);

private:

	// Used for checking connection established timeout
	virtual void 		OnTimeOut(OpTimer* timer);

#ifdef SYNCHRONOUS_HOST_RESOLVING
	void				HandleSynchronousDNS();
#endif // SYNCHRONOUS_HOST_RESOLVING

	CommState			ConnectAltSocket();
	CommState			StartLoading();
	virtual void		EndLoading() {}

	virtual void		SetRequestMsgMode(ProgressState parm)
		{
#ifdef HAS_SET_HTTP_DATA
			info.is_uploading = (parm == UPLOADING_FILES);
#endif
			m_RequestMsgMode = parm;
		}
	virtual void		SetConnectMsgMode(ProgressState parm) { m_RequestMsgMode = parm; }
	virtual void		SetAllSentMsgMode(ProgressState parm, ProgressState request_parm);

	/** Look for this object in g_ConnectionWaitList.
	 *
	 *  @return  CommWaitElm responsible for deletion of this Comm object,
	 *           or NULL if it's not found.
	 */
	CommWaitElm*		FindInConnectionWaitList() const;

	/** Check if this object is marked as deleted. */
	virtual BOOL		IsDeleted() const;

  public:

	virtual void		CloseConnection() { CloseSocket(); }

#ifdef COMM_LOCALHOSTNAME
# ifdef COMM_LOCALHOSTNAME_IS_SERVERNAME
	static ServerName*
# else
	static const char*
# endif
		GetLocalHostName();
#endif // COMM_LOCALHOSTNAME

	virtual void		SetDoNotReconnect(BOOL val);
	virtual BOOL		PendingClose() const;

#ifdef _EXTERNAL_SSL_SUPPORT_
public:
	virtual void UpdateSecurityInformation() { SComm::SetProgressInformation(SET_SECURITYLEVEL, SECURITY_STATE_NONE, UNI_L("")); }
#ifdef URL_ENABLE_INSERT_TLS
	virtual BOOL InsertTLSConnection(ServerName *_host, unsigned short _port) { OP_ASSERT(!"This call should have ended up in External_SSL_Comm"); return FALSE; } 
#endif
	void SetSecure(BOOL sec) { info.is_secure = sec; };
#ifdef ABSTRACT_CERTIFICATES
	virtual OpCertificate* ExtractCertificate() { return NULL;}
#endif // ABSTRACT_CERTIFICATES
#endif // _EXTERNAL_SSL_SUPPORT_

#ifdef WEBSOCKETS_SUPPORT
	void SetIsFullDuplex(BOOL full_duplex) { info.is_full_duplex = full_duplex; }
#endif // WEBSOCKETS_SUPPORT

#ifdef TCP_PAUSE_DOWNLOAD_EXTENSION
	/* Stops doing recv() on the socket */
	virtual void PauseLoading();
	/* Resumes doing recv() on the socket */
	virtual OP_STATUS ContinueLoading();
#endif // TCP_PAUSE_DOWNLOAD_EXTENSION

#ifdef OPSOCKET_GETLOCALSOCKETADDR
	void CloseIfInterfaceDown(OpSocketAddress* local_interface_adress, OpSocketAddress* server_address);
#endif // 	OPSOCKET_GETLOCALSOCKETADDR

protected:

	void SocketDataSent();

	// From OpHostResolverListener
	virtual void OnHostResolved(OpHostResolver* aHostResolver);
	virtual void OnHostResolverError(OpHostResolver* aHostResolver,
		                     OpHostResolver::Error aError);
	// From OpSocketListener
	
#ifdef _EXTERNAL_SSL_SUPPORT_
	virtual BOOL OnSocketSecurityProblem(OpSocket* socket, /* problem, certificate, cipher, */ BOOL &later){ OP_ASSERT(!"should go to External_SSL_Comm"); return FALSE; } 
#endif	
	
	virtual void OnSocketConnected(OpSocket* aSocket);
	virtual void OnSocketDataReady(OpSocket* aSocket);
	virtual void OnSocketDataSent(OpSocket* aSocket, UINT aBytesSent);
#ifndef URL_TRUST_ONSOCKETDATASENT
	virtual void OnSocketDataSendProgressUpdate(OpSocket* aSocket, UINT aBytesSent);
#endif // URL_TRUST_ONSOCKETDATASENT
	virtual void OnSocketClosed(OpSocket* aSoocket);
	virtual void OnSocketConnectionRequest(OpSocket* aSocket);
	virtual void OnSocketConnectError(OpSocket* aSocket, OpSocket::Error aError);
	virtual void OnSocketListenError(OpSocket* aSocket, OpSocket::Error aError){}
	virtual void OnSocketReceiveError(OpSocket* aSocket, OpSocket::Error aError);
	virtual void OnSocketSendError(OpSocket* aSocket, OpSocket::Error aError);
	virtual void OnSocketCloseError(OpSocket* aSocket, OpSocket::Error aError);
#ifdef CORESOCKETACCOUNTING    
    virtual void OnSocketInstanceNumber(int aInstanceNumber);
#endif // CORESOCKETACCOUNTING
#ifdef _EMBEDDED_BYTEMOBILE
public:
	OP_STATUS SetZLibTransceive();
#endif //_EMBEDDED_BYTEMOBILE
};

#ifdef NETWORK_STATISTICS_SUPPORT

#define NUM_LOG_POINTS 60

class Network_Statistics_Manager
{
	struct network_log_point
	{
		time_t last_activity;
		int transfered;
	};

	network_log_point log_points[NUM_LOG_POINTS];
	UINT64 total_sent;
	UINT64 total_received;
	int max_transfer_speed_total;
	network_log_point max_transfer_speed_last_ten_minutes[10];
	BOOL freeze;
	double dns_delays;
	int dns_delay_count;

	void addBytesTransferred(int amount);

public:
	Network_Statistics_Manager();
	void addBytesSent(int amount);
	void addBytesReceived(int amount);
	int getTransferSpeedMaxTotal();
	int getMaxTransferSpeedLastTenMinutes();
	int getBytesTransferredLastTenMinutes();
	UINT64 getBytesTransferred();
	UINT64 getBytesSent();
	UINT64 getBytesReceived();
	void Reset();
	void Freeze();
#ifdef OPERA_PERFORMANCE
	void WriteReport(URL &url);
#endif // OPERA_PERFORMANCE
#if defined (OPERA_PERFORMANCE) || defined (SCOPE_RESOURCE_MANAGER)
	void addDNSDelay(double amount) { dns_delays += amount; dns_delay_count++; };
#endif // OPERA_PERFORMANCE || SCOPE_RESOURCE_MANAGER
};
#endif // NETWORK_STATISTICS_SUPPORT

#endif
