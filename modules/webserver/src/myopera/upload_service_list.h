/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2007 - 2008
 *
 * Web server implementation 
 */

#ifndef UPLOAD_SERVICE_LIST_H_
#define UPLOAD_SERVICE_LIST_H_

#include "modules/hardcore/timer/optimer.h"
#include "modules/util/adt/opvector.h"
#include "modules/url/url2.h"
#include "modules/windowcommander/OpWindowCommander.h"
#include "modules/windowcommander/OpTransferManager.h"
#include "modules/windowcommander/src/TransferManager.h"


class WebSubServer;
class OpTransferItem;
class WebserverEventListener;

class UploadServiceListManager
	: public OpTimerListener 
	, public OpTransferListener	
{
public:	
	static OP_STATUS Make(UploadServiceListManager *&uploader, const uni_char *upload_service_uri, const uni_char *device_uri, const uni_char *devicename, const uni_char *username, WebserverEventListener *listener);
	
	OP_STATUS Upload(OpAutoVector<WebSubServer> *subservers, const char *upload_service_session_token);
	
	virtual ~UploadServiceListManager();
private:
	UploadServiceListManager(WebserverEventListener *listener);

	// OpTransferListener
	void OnProgress(OpTransferItem* transferItem, TransferStatus status);
	void OnReset(OpTransferItem* transferItem);
	void OnRedirect(OpTransferItem* transferItem, URL* redirect_from, URL* redirect_to);
	
	virtual void OnTimeOut(OpTimer* timer);
	
	OpTransferItem	*m_transferItem;
	WebserverEventListener *m_event_listener;
	
	BOOL			m_in_progress;
	BOOL			m_lingering_upload;
	BOOL			m_authenticated;
	int 			m_retries;
	
	OpString 		m_sentral_server_uri;
	OpString 		m_device_uri;
	OpString 		m_devicename;
	OpString 		m_username;
	OpTimer 		m_timer;
	BOOL			m_firstTimeout;
	OpString8		m_xml_data;
};

#endif /*UPLOAD_SERVICE_LIST_H_*/
