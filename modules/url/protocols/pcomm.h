/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef _URL2_PCOMM_
#define _URL2_PCOMM_

#include "modules/url/protocols/scomm.h"

class ServerName;

/** This class is the base class of implementations that only processes 
 *	data before sending them on to another processing stage (the sink), or 
 *	processes data received from the sink before sending them on to the parent
 *
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
 *			The receiver may act on the error code, or send it on tho the next parent.
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
class ProtocolComm : public SComm
{
private:

	/** The sink object (target for sending/source for reading) */
	SComm*				sink;

public:

	/** Constructor 
	 *
	 *	@param	msg_handler		Message handler that will be used to post messages and register callbacks
	 *	@param	prnt			Pointer to the parent of this object 
	 */
						ProtocolComm(MessageHandler* msg_handler, SComm *prnt, SComm *snk);

	/** Destructor */
	virtual				~ProtocolComm();

	/** Is this object safe to delete? */
	virtual BOOL		SafeToDelete() const;

	/** Replace the sink of this object with a new sink */
	virtual void		SetNewSink(SComm *snk);

	/** remove the curent sink, leaving this object without a sink */
	virtual	SComm*		PopSink();

	/** Set callbacks for the parent and the master parent object.
	 *	Usually the callbacks should be registered as (message, Id())
	 *
	 *	@param	master	The top master of the parent (some messages may need to go directly)
	 *	@param	parent	pointer to the parent.
	 */
	virtual OP_STATUS	    SetCallbacks(MessageObject* master, MessageObject* parent);

	/** Start loading, return status of operation (so far) */
	virtual CommState	Load();

	/** Implementationspecific request to read data from parent.
	 *	ProtocolComm::SignalReadData() will call sink->SignalReadData() with the given buffer as input
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
	virtual unsigned int ReadData(char* buf, unsigned blen);

	/** Send the specified data through the implementation classes network implementation
	 *	This function may be called at any time by the parent, and the implementation MUST 
	 *	be able to handle situations where it cannot process the data fast enough
	 *
	 *	NOTE: This object takes ownership of the input buffer, and MUST delete it after use
	 *	NOTE: The ProtocolComm::SendData function forwards the buffer to the sink, or deletes it
	 *	
	 *	@param	buf		buffer to send
	 *	@param	blen	length of buffer
	 */
	virtual void		SendData(char *buf, unsigned blen); // buf is always freed by the handling object

	virtual void 		SetMaxTCPConnectionEstablishedTimeout(UINT timeout);
	virtual void		CloseConnection();	// Controlled shutdown
	virtual BOOL		Connected() const;
	virtual BOOL		Closed();
	virtual void		Stop();				// Cut connection/reset connection
	virtual void		UpdateSecurityInformation(){if(sink) sink->UpdateSecurityInformation(); else SComm::UpdateSecurityInformation();}
	virtual int			GetSecurityLevel() const {return sink ? sink->GetSecurityLevel(): SComm::GetSecurityLevel();}
	virtual const uni_char *GetSecurityText() const {return  sink ? sink->GetSecurityText(): SComm::GetSecurityText();}

	virtual void		SetRequestMsgMode(ProgressState parm);
	virtual void		SetConnectMsgMode(ProgressState parm);
	virtual void		SetAllSentMsgMode(ProgressState parm, ProgressState request_parm);
	virtual	SComm		*GetSink();
	virtual BOOL		HasId(unsigned int sid);

	virtual void		SetDoNotReconnect(BOOL val);
	virtual BOOL		PendingClose() const;
#ifdef _SSL_SUPPORT_
#ifdef URL_ENABLE_INSERT_TLS
	virtual BOOL		InsertTLSConnection(ServerName *_host, unsigned short _port);
#endif
#ifdef ABSTRACT_CERTIFICATES
	virtual OpCertificate* ExtractCertificate() { return sink ? sink->ExtractCertificate() : 0; }
#endif // ABSTRACT_CERTIFICATES
#endif //_SSL_SUPPORT_

#ifdef TCP_PAUSE_DOWNLOAD_EXTENSION
	virtual void        PauseLoading();
	virtual OP_STATUS   ContinueLoading();
#endif // TCP_PAUSE_DOWNLOAD_EXTENSION

#ifdef URL_NOTIFY_FILE_DOWNLOAD_SOCKET
	virtual void		SetIsFileDownload(BOOL value) { if( sink ) sink->SetIsFileDownload(value); }
#endif

};

#endif /* _PCOMM_ */
