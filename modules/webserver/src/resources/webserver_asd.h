/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2006 - 2008
 *
 * Web server implementation
 */
 
#ifndef WEBSERVER_ASD_H_
#define WEBSERVER_ASD_H_
#if defined(WEBSERVER_SUPPORT)

#include "modules/webserver/webserver_resources.h"
#include "modules/webserver/webresource_base.h"

class WebServerConnection;
class WebServer;

/**
  This class provide a sort of "replica" of the ASD service "list of Services for a user" (https://ssl.opera.com:8008/developerwiki/Alien/Service_Discovery_API_(REST)) for details
*/
class WebResource_ASD: public WebResource_Administrative_Standard
{
private:
	/** Create the XML that provide the list of services.
	Example:
		<services length="3" total="3">
			<service access="public" author="Opera Software ASA" description="Communicate with your friends in My Opera in a one-to-one, live session." device="opera" name="Messenger" onlinefor="" originurl="http://unite.opera.com/bundled/messenger/" path="messenger" server="opera.lucav.operaunite.com" username="lucav"/>
			<service access="public" author="Mathieu Henri, Opera Software ASA" description="Share your personal photos with friends around the world without the need to upload them." device="opera" name="Photo Sharing" onlinefor="" originurl="http://unite.opera.com/widget/download/82/" path="photo_sharing" server="opera.lucav.operaunite.com" username="lucav"/>
			<service access="public" author="Mathieu Henri, Opera Software ASA" description="A simple and safe way to share files directly from your computer." device="ubuntu" name="Unite - Salsa and Bachata" onlinefor="" originurl="http://unite.opera.com/bundled/fileSharing/" path="file_sharing" server="ubuntu.lucav.operaunite.com" username="lucav"/>
		</services>
	*/
	OP_STATUS CreateServicesXML();
	
public:
	/* Create the resource */
	static OP_STATUS Make(WebResource *&webresource, WebServerConnection *service, const OpStringC16 &subServerRequestString, WebserverRequest *requestObject, WebserverResourceDescriptor_Base *resource, WebServerFailure *result);
	
	WebResource_ASD(WebServerConnection *service, WebserverRequest *requestObject): WebResource_Administrative_Standard(service, requestObject) { }
};

#endif // defined(WEBSERVER_SUPPORT)
#endif /*WEBSERVER_FILE_H_*/
