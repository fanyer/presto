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

#ifndef _URL2_SCOMM_
#define _URL2_SCOMM_

#include "modules/hardcore/mh/mh_enum.h"
class MessageHandler;
#include "modules/hardcore/mh/messobj.h"

#include "modules/util/objfactory.h"
#include "modules/util/simset.h"

#include "modules/url/url_enum.h"
#include "modules/url/url_sn.h"
#include "modules/url/protocols/commelm.h"

#ifdef ABSTRACT_CERTIFICATES
#include "modules/pi/network/OpCertificate.h"
#endif // ABSTRACT_CERTIFICATES

#define COMM_STILL_CONNECTING       0x00
#define COMM_CONNECTED              0x01

/** Stop unsent requests in all open connection, move to new connections */
#define STOP_REQUESTS_ALL_CONNECTIONS 0x00
/** Stop unsent requests in this connection, move to new connections */
#define STOP_REQUESTS_THIS_CONNECTION 0x01
/** Stop unsent requests in this connection, but the first request will be allowed even if unsent, move to new connections */
#define STOP_REQUESTS_TC_NOT_FIRST_REQUEST 0x02

class ServerName;

/** The basic network communication class
 *
 *	Subclasses of SComm receives orders and data from the parent, and signals 
 *	status and received data to the parent.
 *
 *  !!!NOTE!!!NOTE!!! ALL OBJECTS DERIVED FROM SComm MUST BE
 *	DESTROYED USING THE FUNCTION SComm::SafeDestruction()
 *	FAILURE TO DO SO MAY CAUSE CRASHES.
 *
 *	NOTE: Objects of this class MUST be destroyed using the SafeDelete() function
 *	Messages usually sent by SComm/ProtocolComm implementations (specific implementations may add others)
 *
 *		MSG_COMM_LOADING_FINISHED:
 *			par1 : sender Id
 *			par2 : unused
 *
 *			Indicates that the sink received a normal close from the server (or have decided to treat it as such)
 *
 *		MSG_COMM_LOADING_FAILED:
 *			par1 : sender Id
 *			par2 : error code
 *
 *			An error occured in the sink, par2 contains the languagecode for the error.
 *			The sink is usually closed when these messages are sent, the exception being 
 *			Str::SI_ERR_HTTP_100CONTINUE_ERROR, which is non-fatal
 *
 *			The receiver may act on the error code, or send it on to the next parent.
 *
 *			The error code ERR_SSL_ERROR_HANDLED indicates that an error occured, but 
 *			has been handled, and the parent/document should silently end loading.
 *
 *		MSG_COMM_DATA_READY
 *			par1 : sender Id
 *			par2 : unused
 *
 *			Fallback message, used in some cases by ProcessReceivedData when an SComm does not 
 *			have an SComm derived parent, to indicate that there are data waiting to be read.
 *
 *
 *	Implementations should implement the following functions:
 *		
 *		SetCallbacks()
 *		Load()
 *		ReadData()
 *		SendData()
 *		ConnectionEstablished()
 *		RequestMoreData()
 *		ProcessReceivedData()
 *		SetProgressInformation()
 *		Stop()
 *		HandleCallback()
 *
 *
 *	And may implement, as needed:
 *
 *		SecondaryProcessReceivedData()
 *		GetSecurityLevel()
 *		GetSecurityText()
 *		SafeToDelete()
 *
 *	SetProgressInformation should consider handling of the following progress_levels
 *
 *		STOP_FURTHER_REQUESTS : Should not send further requests on this connection 
 *			(sent by SSL for some servers that do not handle renegotiation gracefully)
 *			Parameter-value can be STOP_REQUESTS_ALL_CONNECTIONS, STOP_REQUESTS_THIS_CONNECTION,
 *				STOP_REQUESTS_TC_NOT_FIRST_REQUEST
 *		RESTART_LOADING: The request should be resent, Progress_info2 (if non-NULL) 
 *			is a pointer to a BOOL that must be set to TRUE if the operation is permitted. 
 *			(Used by SSL in case the secure connection has to be restarted, during initial handshake)
 *		HANDLE_SECONDARY_DATA: Used by HTTPS through proxy to indicate that the real source 
 *			of data is the CONNECT request to the proxy, not the actual request
 *		HEADER_LOADED : Inidicates that the header has been loaded, or the connection is 
 *			ready to receive payload data.
 *		GET_APPLICATIONINFO : Request by sink (usually SSL) to get the current URL for 
 *			display purposes. The Password part of the URL MUST be hidden. progress_info2 
 *			is a pointer to a OpString pointer.
 */
class SComm
  : public MessageObject
{
private:
	friend class URL_Manager;

	SComm*				parent;

	/** Look for this object in g_DeletedCommList.
	 *
	 *  @return  SCommWaitElm responsible for deletion of this SComm object,
	 *           or NULL if it's not found.
	 */
	SCommWaitElm*		FindInDeletedCommList() const;

	/** Check if this object is marked as deleted. */
	virtual BOOL		IsDeleted() const;

protected:

	/** call_count incrementor, Already ANCHORed */
	class AnchoredCallCount : public CleanupItem{
	private:
		SComm *object;
	public:
		AnchoredCallCount(SComm *trg){OP_ASSERT(trg); object=trg; object->call_count++;}
		virtual ~AnchoredCallCount(){object->call_count--;}
	};

	/** call_count incrementor, NOT anchored */
	class NormalCallCount{
	private:
		SComm *object;
	public:
		NormalCallCount(SComm *trg){OP_ASSERT(trg); object=trg; object->call_count++;}
		virtual ~NormalCallCount(){object->call_count--;}
	};

	friend class AnchoredCallCount;
	friend class NormalCallCount;

	/** Message handler of this object */
	MessageHandler*		mh;

	/** pseudo unique ID, used for callbacks */
	unsigned int		id;

	/** How many nested calls out of this object are there (used to prevent destruction of objects in use on the callstack */
	int call_count;

	/** Is this object actively connected to a "server"? */
	BOOL				is_connected;
#ifdef NEED_URL_ABORTIVE_CLOSE	
	BOOL				m_abortive_close;
#endif

public:

	/** Constructor 
	 *
	 *	@param	msg_handler		Message handler that will be used to post messages and register callbacks
	 *	@param	prnt			Pointer to the parent of this object 
	 */
						SComm(MessageHandler* msg_handler, SComm *prnt);

	/** Destructor */
	virtual				~SComm();

	/** Change parent for this object 
	 *
	 *	@param	prnt	Pointer to the new parent of this object 
	 */
	void				ChangeParent(SComm *prnt){parent = prnt;};

	/** The Id of this object */
	unsigned int		Id() const { return id; };

	/** The ID of the parent */
	unsigned int		ParentId() {return parent ? parent->Id() : 0;};

	/** The callback handler (must be defined by subclasses)
	 *
	 *	@param	msg		The message that triggered the callback
	 *	@param	par1	message specific argument (usually the Id of the sender)
	 *	@param	par2	message specific argument of any type.
	 */
	virtual void		HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2) = 0;

	/** Set callbacks for the parent and the master parent object.
	 *	Usually the callbacks should be registered as (message, Id())
	 *
	 *	@param	master	The top master of the parent (some messages may need to go directly)
	 *	@param	parent	pointer to the parent.
	 */
	virtual OP_STATUS	SetCallbacks(MessageObject* master, MessageObject* parent) = 0;

	/** Reset parent. Is this still in use?*/
	void				Clear();

#ifdef NEED_URL_ABORTIVE_CLOSE
	void				SetAbortiveClose() { m_abortive_close = TRUE; }
#endif

	/** Start loading, return status of operation (so far) */
	virtual CommState	Load() = 0;

	/** Set tcp connection timeout.
	 *
	 * Note that most connections attempts will be prevented if network is down.
	 *
	 * This timeout will catch attempts when network is not detecable on local device,
	 * For example if the local wifi router goes offline.
	 *
	 * If a connection has not been established after timeout_seconds the connection
	 * will fail with error MSG_COMM_LOADING_FAILED and parameter ERR_NETWORK_PROBLEM.
	 *
	 * All ongoing connections to the same server will also be failed, as they otherwise will hang.
	 *
	 * @param timeout_seconds The timeout in milliseconds. Note that timeout == 0 has no effect.
	 */
	virtual void SetMaxTCPConnectionEstablishedTimeout(UINT timeout_seconds) = 0;

	/** Used to tell this object that it a connection has been established and that it may start working on its part of the job
	 *	If this object is already connected, signal parent, otherwise call ConnectionEstablished()
	 *
	 *	@return	CommState	Status of operation
	 */
			CommState	SignalConnectionEstablished();
	/** Signals the subclass that the connection has been established and that it should start its job.
	 *	Implementations may call ConnectionEstablished of its base class immediately, or at some later time when its state 
	 *	indicates that it should signal the parent that its connection has been established.
	 *
	 *	SComm::ConnectionEstablished() will signal to the parent that the connection has been established
	 *
	 *	@return	CommState	Status of operation
	 */
	virtual CommState	ConnectionEstablished();

	/** Send the specified data through the implementation classes network implementation
	 *	This function may be called at any time by the parent, and the implementation MUST 
	 *	be able to handle situations where it cannot process the data fast enough
	 *
	 *	NOTE: This object takes ownership of the input buffer, and MUST delete it after use
	 *	
	 *	@param	buf		buffer to send
	 *	@param	blen	length of buffer
	 */
	virtual void		SendData(char *buf, unsigned blen) = 0; // buf is always freed by the handling object

	/** Signal to parent that this object may send more data
	 *	Upon receving this call the parent should call SendData if it 
	 *	is has more data waiting to be sent.
	 */
	virtual void		RequestMoreData();

	/** Implementationspecific request to read data from parent.
	 *	SComm::SignalReadData() will call ReadData() with the given buffer as input
	 *
	 *	@param	buf		target buffer, the read data will be put here
	 *	@param	blen	maximum length of target buffer
	 *
	 *	@return	unsigned int	actual number of bytes read into the target buffer
	 */
	virtual unsigned int SignalReadData(char* buf, unsigned blen);

	/** Fill the buffer provided by the parent with data as specified by the implementation
	 *
	 *	@param	buf		target buffer, the read data will be put here
	 *	@param	blen	maximum length of target buffer
	 *
	 *	@return	unsigned int	actual number of bytes read into the target buffer
	 */
	virtual unsigned int ReadData(char* buf, unsigned blen) = 0;

	/** The sink indicates to the parent that there are data waiting to be read
	 *	If this object is connected (or does not have a parent) ProcessReceivedData will be called, 
	 *	otherwise (if the object is not connected) this object will signal its parent that data 
	 *	are waiting to be read
	 */
			void		SignalProcessReceivedData();
	/** This function has two uses: 
	 *		1) There are data waiting to be read in this object's sink object.
	 *
	 *		2) When the state of the implementation indicates it, it calls this function in its baseclass to signal 
	 *		to its parent that there are data waiting to be read
	 *
	 *	SComm::ProcessReceivedData() will call the parent's SignalProcessReceivedData function, unless the 
	 *	parent's sink is not this object, in which case it will call the parent's SecondaryProcessReceivedData
	 */
	virtual void		ProcessReceivedData();

	/** Used by a sink to indicate that data from a secondary sink object are waiting to be read */
	virtual void		SecondaryProcessReceivedData();

    /** Used to retrieve security information from a lower member in the stack */
	virtual void		UpdateSecurityInformation(){};

	/** Retrieve the security level of this object */
	virtual int			GetSecurityLevel() const {return SECURITY_STATE_NONE;}

	/** Retrived the security description text of this object */
	virtual const uni_char *GetSecurityText() const {return NULL;}

	/** Handle progress reports from the sink, and Send progess reports to the parent.
	 *	Implementationas MUST send progress reports they do not handle to the parent 
	 *	through the baseclass implementasion of this function
	 *
	 *	@param	progress_level	Indication of what progress state that the sink has reached
	 *	@param	progress_info1	statespecific information
	 *	@param	progress_info2	statespecific information, usually a string, but may be used as a void pointer to retrieve data
	 */
	virtual void		SetProgressInformation(ProgressState progress_level, unsigned long progress_info1=0, const void *progress_info2 = NULL);

	/** Implementations must perform all necessary loading shutdown, then call the baseclass functions EndLoading 
	 *	in order to tell the parent to shutdown
	 */
	virtual void		EndLoading();

	/** Parent indicates to implementation that the connection must be shut down orderly */
	virtual void		CloseConnection() = 0;	// Controlled shutdown

	/** Query from parent if this object is conncected to a server */
	virtual BOOL		Connected() const = 0;

	/** Query from parent if this object is closed */
	virtual BOOL		Closed() = 0;

	/** Order from parent to this object to shut down forcefully, and reset for new load */
	virtual void		Stop();				// Cut connection/reset connection

	/** Query to parent to find out if the parent is actively using the connection 
	 *	Used by SSL
	 */ 
	virtual BOOL		IsActiveConnection() const;

	/** Query to parent to find out if the parent is still going to use the connection
	 *	Used by HTTP_1_1 to determine if connection to https proxy can be closed
	 */ 
	virtual BOOL		IsNeededConnection() const;

	/** Set the progress state indication for sending data in the sink (implementation specific handling) */
	virtual void		SetRequestMsgMode(ProgressState parm) {}
	/** Set the progress state indication while connectin in the sink (implementation specific handling) */
	virtual void		SetConnectMsgMode(ProgressState parm) {}
	/** Set the progress state indication to be sent when all the pending data in the sink have been sent (implementation specific handling)
	 *  If the implementation is not a final Sink, then it must pass this on to the next level below when the buffer is exhausted
	 *	Once signaled or passed on, the mode must be forgotten
	 *  request_parm is the mode to use in SetRequestMsgMode afterwards.
	 */
	virtual void		SetAllSentMsgMode(ProgressState parm, ProgressState request_parm) {}

	/** Is this object safe to delete? */
	virtual BOOL		SafeToDelete() const;

	/** Get the pointer to the parent */
	SComm*				GetParent() { return parent; }

	/** Get the const pointer to the parent */
	const SComm*		GetParent() const { return parent; }

	/** Get the parent's sink pointer */
	virtual	SComm*		GetSink() { return NULL; }

	/** Does this object have the given sink ID? */
	virtual BOOL		HasId(unsigned int sid){return (sid == id);}

	/** Mark this object as deleted (and insert it in the delete list). */
	virtual void		MarkAsDeleted();

	/** Remove the comm objects marked as deleted */
	virtual void		RemoveDeletedComm();

	/** Remove the deleted comm objects */
	static  void		SCommRemoveDeletedComm();

	/** Indicate to this object that it should not attempt to reconnect if the connection fails */
	virtual void		SetDoNotReconnect(BOOL val) = 0;

	/** Are we waiting for the connection to close? */
	virtual BOOL		PendingClose() const = 0;

	/** Destroy the given object safely */
	static  void		SafeDestruction(SComm *target);

#ifdef _EMBEDDED_BYTEMOBILE
	virtual OP_STATUS	SetZLibTransceive() { return OpStatus::OK; }
#endif //_EMBEDDED_BYTEMOBILE

#ifdef _EXTERNAL_SSL_SUPPORT_
#ifdef URL_ENABLE_INSERT_TLS
	/** Insert a TLS connection in the connection pipeline */
	virtual BOOL		InsertTLSConnection(ServerName *_host, unsigned short _port);
#endif // URL_ENABLE_INSERT_TLS

	/** Set secure mode on this connection */
	virtual void		SetSecure(BOOL sec) {};

#ifdef ABSTRACT_CERTIFICATES
	/** Extract the X.509 certificate used to authenticate the server */
	virtual OpCertificate* ExtractCertificate() = 0;
#endif // ABSTRACT_CERTIFICATES
#endif // _EXTERNAL_SSL_SUPPORT_

#ifdef TCP_PAUSE_DOWNLOAD_EXTENSION
	/**	Pause the loading of data on this connection */
	virtual void        PauseLoading() {}
	/** Restart the loading of data on this connection */
	virtual OP_STATUS   ContinueLoading() { return OpStatus::ERR; }
#endif // TCP_PAUSE_DOWNLOAD_EXTENSION

#ifdef URL_DISABLE_INTERACTION
private:
	BOOL user_interaction_blocked;

public:
	BOOL GetUserInteractionBlocked() const;
	void SetUserInteractionBlocked(BOOL flag){user_interaction_blocked = flag;}
#endif

#ifdef URL_NOTIFY_FILE_DOWNLOAD_SOCKET
	virtual void		SetIsFileDownload(BOOL value) {}
#endif

#ifdef URL_PER_SITE_PROXY_POLICY
private:
    // Bypass the hostname check and always route through proxy when TRUE
    BOOL force_socks_proxy;

public:
    void SetForceSOCKSProxy(BOOL value) {force_socks_proxy = value;}
    BOOL GetForceSOCKSProxy();
#endif
};

#endif /* _SCOMM_ */
