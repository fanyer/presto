/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2007
 *
 * Web server implementation, Authentication
 */
 
#ifndef WEBSERVER_AUTH_H_
#define WEBSERVER_AUTH_H_



/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2007 - 2008
 *
 * Web server implementation 
 */


#include "modules/webserver/webserver_resources.h"
#include "modules/formats/hdsplit.h"
#include "modules/webserver/webresource_base.h"

class WebserverAuthSession;
class WebServerConnection;

class WebResource_Auth : public WebResource
{
public:

	~WebResource_Auth();
	static OP_BOOLEAN Make(WebResource *&webResource, const OpStringC16 &subServerRequestString, WebServerConnection *service, WebserverRequest *requestObject, WebserverResourceDescriptor_AccessControl *authElement, WebServerFailure *result);

	/*Implementation of WebResource*/
	virtual OP_STATUS HandleIncommingData(const char *incommingData,unsigned length,BOOL lastData=FALSE);
	virtual OP_BOOLEAN FillBuffer(char *buffer, unsigned int bufferSize, unsigned int &FilledDataSize);
	virtual OP_STATUS GetLastModified(time_t &date) { date = 0; return OpStatus::OK; }


	OP_STATUS StartServingData();	
	OP_STATUS Authenticated(WebserverAuthenticationId::AuthState success, WebserverAuthSession *session, WebSubServerAuthType auth_type);
	OP_STATUS OnConnectionClosed(){ return OpStatus::OK; }
	
	static OP_STATUS RemoveUniteAuthItems(WebserverRequest *requestObject, OpString &request);
	
	void SetAuthenticationMethod(WebserverAuthenticationId *auth_object){m_authentication_object = auth_object; }
private:
	OP_STATUS RedirectToLoginPage(const OpStringC &auth_path, WebSubServerAuthType auth_type, const OpStringC &reason);  
	
	/*methods*/
	WebResource_Auth(WebServerConnection *service, WebserverRequest *requestObject, WebserverResourceDescriptor_AccessControl *authElement, WebSubServerAuthType auth_type);
	OP_STATUS GenerateCookieSessionKeyResponse(WebserverAuthSession *session, OpString &location);

	
	/*data*/	
	
	OpString m_auth_post_data;
	unsigned m_post_data_length;
	
	OpString m_auth_path;
	
	WebSubServerAuthType m_auth_type;
	WebserverResourceDescriptor_AccessControl *m_authElement;
	WebserverAuthenticationId *m_authentication_object;
};


#endif /*WEBSERVER_AUTH_H_*/
