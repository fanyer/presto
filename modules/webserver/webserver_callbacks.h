/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2007 - 2008
 *
 */

#ifndef WEBSERVER_CALLBACKS_H_
#define WEBSERVER_CALLBACKS_H_

/* These listeners are only used by the WebResource_Custom resource */

class WebserverBodyObject_Base;

class Webserver_HttpConnectionListener : public Link
{
	public:
	virtual ~Webserver_HttpConnectionListener() {}
	virtual OP_STATUS WebServerConnectionKilled() = 0;
};


class FlushIsFinishedCallback
{
public:
   virtual OP_STATUS FlushIsFinished(unsigned int flushId) = 0;
   virtual ~FlushIsFinishedCallback(){}
};

class ObjectFlushedCallback
{
public:
   virtual OP_STATUS BodyObjectFlushed(WebserverBodyObject_Base *bodyDataObject) = 0;
   virtual ~ObjectFlushedCallback(){}
};

class WebserverNewRequestCallback
{
public:
   virtual void OnWebserverNewRequest() = 0;
   virtual ~WebserverNewRequestCallback(){}
};

class AddToCollectionCallback
{
public:
   virtual OP_STATUS AddToCollection(const uni_char* key, const uni_char* dataString) = 0;
   virtual ~AddToCollectionCallback(){}
};

class WebserverFolderCallback
{
public:	
	virtual ~WebserverFolderCallback(){}

	virtual OP_STATUS OnFolderSelected(const uni_char* files) = 0;
	virtual OP_STATUS OnCancelled() = 0;	
};


#endif /*WEBSERVER_CALLBACKS_H_*/
