/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2007 - 2008
 *
 */

#ifndef WEBSERVER_MAGIC_URL_H_
#define WEBSERVER_MAGIC_URL_H_

#include "modules/webserver/webserver_user_management.h"
#include "modules/hardcore/timer/optimer.h"

class WebserverAuthenticationId_MagicUrl : public WebserverAuthenticationId
{	
public:
	
	/* Create a magic url.
	 *
	 * Add the object to a WebserverAuthUser after creation. This 
	 * will enable this user to log in using any magic urls created 
	 * with GetMagicUrlString(..)
	 *  
	 * @param auhtentication_object(out) 	The created object. 		
	 * @param auth_id 						The id is used too look up the user. 
	 * 										Must be unique for all objects with type WEB_AUTH_TYPE_HTTP_MAGIC_URL. 
	 * 										Can be the same as the name of the user, where the object is added.
	 * @return OpStatus::ERR_NO_MEMORY		for OOM.
	 * @return OpStatus::OK					If Success.  
	 */
	static OP_STATUS Make(WebserverAuthenticationId_MagicUrl *&auhtentication_object, const OpStringC &auth_id); 
	
	
	/* Get a magic url string. Use this to
	 * create magic url's within services.
	 * 
	 * @param magic_url (out)			The magic url
	 * @param subserver					The service this path is created for.
	 * @param service_sub_path  		The sub path in the service.
	 * @return OpStatus::ERR_NO_MEMORY 	For OOM.
	 */
	OP_STATUS GetMagicUrlString(OpString &magic_url, const WebSubServer *service, const OpStringC &service_sub_path_and_args = UNI_L(""));
	
	/* Get the type of this authentication object */
	WebSubServerAuthType GetAuthType() const { return WEB_AUTH_TYPE_HTTP_MAGIC_URL; }
	
protected:
	OP_STATUS CheckAuthentication(WebserverRequest *request_object);
	
	/* Used for persistent storage.*/
	static OP_STATUS MakeFromXML(WebserverAuthenticationId_MagicUrl *&auhtentication_object, XMLFragment &auth_element);		
	virtual OP_STATUS CreateAuthXML(XMLFragment &auth_element);
private:
	
	WebserverAuthenticationId_MagicUrl();
	
	OpString m_auth_data;
	friend class WebserverAuthenticationId;
};

#endif /*WEBSERVER_MAGIC_URL_H_*/
