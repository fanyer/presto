/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2006 - 2008
 *
 * Web server implementation
 */
 
#ifndef WEBSERVER_FILE_H_
#define WEBSERVER_FILE_H_

#include "modules/webserver/webserver_resources.h"
#include "modules/webserver/webserver_filename.h"
#include "modules/webserver/webresource_base.h"
#include "modules/util/opfile/opfile.h"

class WebServerConnection;

class WebResource_File : public WebResource
{
public:
 
	/* FIXME: Put in updated comments here */
	static OP_STATUS Make(WebResource *&webresource, WebServerConnection *service, const OpStringC16 &subServerRequestString, WebserverRequest *requestObject, WebserverResourceDescriptor_Static *resource, WebServerFailure *result);
	
	/* Close the file */
	virtual ~WebResource_File();
	
	/* Serve data from the open file, until EOF */
	virtual OP_BOOLEAN FillBuffer(char *buffer, unsigned int bufferSize, unsigned int &FilledDataSize);

	virtual OP_STATUS HandleIncommingData(const char *incommingData, UINT32 length, BOOL lastData=FALSE);
	
	virtual OP_STATUS GetLastModified(time_t &date);

	virtual OP_STATUS GetETag(OpString8 &etag);

	OP_STATUS OnConnectionClosed(){ return OpStatus::OK; }	
private:
	WebResource_File(WebServerConnection *service, OpFile *file, WebserverRequest *requestObject);

	OpFile *m_file;
	// Number of bytes served by this resource (used for range requests)
	OpFileLength m_bytes_served;
};

#endif /*WEBSERVER_FILE_H_*/
