/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2007 - 2008
 *
 */

#ifndef WEBSERVER_OPENID_H_
#define WEBSERVER_OPENID_H_

#include "modules/webserver/webserver_user_management.h"
#include "modules/hardcore/timer/optimer.h"
//#include "modules/util/adt/opvector.h"
#include "modules/windowcommander/OpWindowCommander.h"
#include "modules/windowcommander/OpTransferManager.h"

class WebserverAuthenticationId_OpenId 
	: public WebserverAuthenticationId
	, public OpTimerListener 
	, public OpTransferListener	
{	
public:
	~WebserverAuthenticationId_OpenId();

	/* Create an openid 
	 * Add the object to a WebserverAuthUser after creation. This 
	 * will enable this user to log in using openid_url.
	 * 
	 * @param auhtentication_object(out) 	The created object. 		
	 * @param openid_url					The unique openid url for this user. For example 'username.myopenid.com' or 'username.my.opera.com'
	 * 										Must be unique for all objects with type WEB_AUTH_TYPE_HTTP_OPEN_ID. 
	 *
	 * @return OpStatus::ERR_NO_MEMORY		for OOM.
	 * @return OpStatus::OK					If Success.  
	 */
	static OP_STATUS Make(WebserverAuthenticationId_OpenId *&auhtentication_object, const OpStringC &openid_url); 
	                             
	virtual WebSubServerAuthType GetAuthType() const { return WEB_AUTH_TYPE_HTTP_OPEN_ID; }

protected:
	static OP_STATUS MakeFromXML(WebserverAuthenticationId_OpenId *&auhtentication_object, XMLFragment &auth_element);
	virtual OP_STATUS CreateAuthXML(XMLFragment &auth_element);

	virtual OP_STATUS CheckAuthentication(WebserverRequest *request_object);
		
	/* OpTransferListener */
	virtual void OnProgress(OpTransferItem* transferItem, TransferStatus status);
	virtual void OnReset(OpTransferItem* transferItem);
	virtual void OnRedirect(OpTransferItem* transferItem, URL* redirect_from, URL* redirect_to);

	/* OpTimerListener */
	virtual void OnTimeOut(OpTimer* timer);
	
private:
	
	OP_STATUS SendRequest(HTTP_Method method, const OpStringC &url);
	OP_STATUS RedirectToOpenidProvider(const OpStringC &openid_provider_xml);
	enum State
	{
		NONE,
	 	REDIRECT_OPENID_SERVER,
	 	CHECK_WITH_OPENID_SERVER
	} m_state;

	OP_STATUS CheckAuthenticationStep1(WebserverRequest *request_object);
	OP_STATUS CheckAuthenticationStep2(WebserverRequest *request_object);
	
	WebserverAuthenticationId_OpenId();
	OpString m_openid_server;
	OpString m_openid_return_to_path;
	OpTransferItem	*m_transferItem;
	WebserverRequest *m_request_object;
	BOOL m_contacting_id_server;
	
	OpTimer 		m_timer;
	
	friend class WebserverAuthenticationId;
};

#endif /*WEBSERVER_OPENID_H_*/
