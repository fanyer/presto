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

#ifndef URL_EXLH_H
#define URL_EXLH_H

#include "modules/url/protocols/http1.h"
#include "modules/url/url2.h"
#include "modules/url/protocols/pcomm.h"
//#include "argsplit.h"
//#include "hdsplit.h"
//#include "scomm.h"

// External request LoadHandler

struct HTTP_request_st;
class URL_External_Request_Manager;
class Upload_BinaryBuffer;

class URL_External_Request_Handler : public Link, public ProtocolComm
{
private:
	URL url;
	URL_External_Request_Manager *manager;
	HTTP_Request *http;
	class Upload_AsyncBuffer *upload_elm;
	char *header_string;
	BOOL waiting;


private:

	HTTP_request_st *GetServerAndRequest(const uni_char *use_proxy, BOOL override_proxy_prefs);
	void		CheckForCookiesHandledL();
	void		HandleHeaderLoaded();
	void		HandleLoadingFailed(MH_PARAM_1 par1, MH_PARAM_2 par2);
	void		HandleLoadingFinished();
	void		DeleteComm();

public:
				URL_External_Request_Handler(MessageHandler* msg_handler, URL_External_Request_Manager *mngr);
				~URL_External_Request_Handler();

	CommState	Load(
#ifdef MULTIPLE_NETWORK_LINKS_SUPPORT
		const uni_char *network_link_name,
#endif
		const char *header, unsigned long header_len, unsigned long body_len,
		const uni_char *proxy, BOOL override_proxy_prefs);
	BOOL		RetrieveData(char *&header, unsigned &header_len,
						unsigned char *&body, unsigned long &body_len);
	void		SendData(const void *data, unsigned long data_len);
	void		EndLoading();
	OP_STATUS	SetCallbacks(MessageObject* master, MessageObject* parent);
	void		SetProgressInformation(ProgressState progress_level, unsigned long progress_info1, const uni_char *progress_info2);
	void		ProcessReceivedData();
	void		HandleCallback(int msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
};

class MURLExternalRequestManager {
public:
	virtual void DataReady(unsigned long id)=0;
	virtual void DataCorrupt(unsigned long id)=0;
	virtual void LoadFailed(unsigned long id, unsigned int errorcode)=0;
	virtual void UploadProgress(unsigned long id, unsigned long bytes)=0;
};

class URL_External_Request_Manager : public MessageObject
{

	friend class URL_External_Request_Handler;

private:
	Head	requests;
	MURLExternalRequestManager *iObserver;

	URL_External_Request_Handler *FindRequest(unsigned long id);

	void HandleCallback(int msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
	void ProcessReceivedData(unsigned long id);
	void RestartingRequest(unsigned long id);

public:

	URL_External_Request_Manager(MURLExternalRequestManager *aObserver);
	~URL_External_Request_Manager();

	// Buffers are NOT deallocated and must be freed by caller
	// Header buffer MUST be HTTP 1.1 compliant and MUST be null terminated
	// HTTP version ignored, some HTTP methods are not accepted
	// URL MUST follow method and MUST be complete URL of http, https, ftp, gopher or wais type
	// some headers may be overridden
	unsigned long LoadRequest(
#ifdef MULTIPLE_NETWORK_LINKS_SUPPORT
		const uni_char *network_link_name,
#endif
		const char *header, unsigned long header_len, unsigned long body_len,
		const uni_char *proxy=0, BOOL override_proxy_prefs=FALSE);


	// Buffers are allocated and must be freed by caller
	// Returns TRUE if more data (even 0 bytes) are expected;
	BOOL RetrieveData(unsigned long id, char *&header, unsigned &header_len,
							unsigned char *&body, unsigned long &body_len);

	/** Send some, or all of, the body data. */
	void SendData(unsigned long id, const void *data, unsigned long data_len);


	void EndLoading(unsigned long id);

private:
};

#endif	// URL_EXLH_H
