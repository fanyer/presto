/* -*- mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 2006-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** @author Haavard Molland haavardm@opera.com
*/

#ifndef DOM_DOMWEBSERVER_H
#define DOM_DOMWEBSERVER_H

#include "modules/dom/src/domobj.h"

#ifdef WEBSERVER_SUPPORT

#include "modules/dom/src/domevents/domeventtarget.h"
#include "modules/dom/src/domevents/domevent.h"
#include "modules/webserver/webserver_custom_resource.h"
#include "modules/dom/src/domcore/domstringlist.h"

class DOM_WebServerConnection;
class DOM_WebServerRequest;
class DOM_WebServerResponse;
class DOM_WebServerCollection;
class DOM_WebServerHeader;
class DOM_WebServer;
class DOM_WebServerArray;
class DOM_WebServerSession;
class DOM_WidgetIcon;
class DOM_WebServerObjectArray;

#ifdef DOM_GADGET_FILE_API_SUPPORT
class DOM_GadgetFile;
#endif


class DOM_WebServerRequestEvent
	: public DOM_Event
{
public:
	static OP_STATUS CreateAndSendEvent(const uni_char *type, DOM_WebServer *webserver, WebResource_Custom *web_resource_script);

	virtual ~DOM_WebServerRequestEvent();

	/* From DOM_Object: */
	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_WEBSERVER_REQUEST_EVENT || DOM_Object::IsA(type); }
	virtual void GCTrace();

private:
	DOM_WebServerRequestEvent(DOM_WebServerConnection *connection);

	DOM_WebServerConnection *m_connection;
};

class DOM_WebServerConnectionClosedEvent
	: public DOM_Event
{
public:
	static OP_STATUS CreateAndSendEvent(unsigned int connection_id, DOM_WebServer *webserver);

	virtual ~DOM_WebServerConnectionClosedEvent();

	/* From DOM_Object: */
	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_WEBSERVER_CONNECTION_CLOSED_EVENT || DOM_Object::IsA(type); }

private:
	DOM_WebServerConnectionClosedEvent(unsigned int connection_id);

	unsigned int m_connection_id;
};

class DOM_WebServerConnection : public DOM_Object, private Webserver_HttpConnectionListener
{
public:

	static OP_STATUS Make(DOM_WebServerConnection *&connection, DOM_WebServer *webserver, DOM_Runtime *runtime, WebResource_Custom *web_resource_script);

	virtual ~DOM_WebServerConnection();

	unsigned int GetConnectionId() { return m_connection_id; }
	const OpString &GetProtocol() { return m_protocol; }

	DOM_WebServer *GetDomWebserver() { return m_webserver; }

	/* From Webserver_HttpConnectionListener: */
	virtual OP_STATUS WebServerConnectionKilled();

	/* From DOM_Object: */
	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_WEBSERVER_CONNECTION || DOM_Object::IsA(type); }
	virtual void GCTrace();


private:
	DOM_WebServerConnection(WebResource_Custom *web_resource_script, DOM_WebServer *webserver);
	BOOL m_connection_is_alive;

	DOM_WebServer *m_webserver;

	DOM_WebServerRequest *m_request;
	DOM_WebServerResponse *m_response;

	unsigned int m_connection_id;
	OpString m_protocol;

	WebResourceWrapper m_web_resource_script;
	WebResource_Custom *GetWebResourceCustom() { return static_cast<WebResource_Custom *>(m_web_resource_script.Ptr()); }
};

class DOM_WebServerRequest
	: public DOM_Object
{
public:

	static OP_STATUS Make(DOM_WebServerRequest *&request, DOM_Runtime *runtime, WebResource_Custom *web_resource_script, DOM_WebServerConnection *connection);

	/* From DOM_Object: */
	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_WEBSERVER_REQUEST || DOM_Object::IsA(type); }
	virtual void GCTrace();

	DOM_DECLARE_FUNCTION(getRequestHeader);
	DOM_DECLARE_FUNCTION(getItem);

	enum {
		FUNCTIONS_getRequestHeader = 1,
		FUNCTIONS_getItem,
		FUNCTIONS_getUploadedFile,
		FUNCTIONS_ARRAY_SIZE
	};

private:
	DOM_WebServerRequest(WebResource_Custom *web_resource_script, DOM_WebServerConnection *connection);

	WebResourceWrapper m_web_resource_script;
	WebResource_Custom *GetWebResourceCustom() { return static_cast<WebResource_Custom *>(m_web_resource_script.Ptr()); }

	DOM_WebServerCollection *m_header_collection;
	DOM_WebServerCollection *m_uri_item_collection;
	DOM_WebServerCollection *m_body_item_collection;
	DOM_GadgetFile *m_uploaded_file_array;
	DOM_WebServerConnection *m_connection;
	DOM_WebServerSession *m_session;
};

class DOM_WebserverStringArray
	: public DOM_DOMStringList,
	  public DOM_DOMStringList::Generator
{
public:
	static OP_STATUS Make(DOM_WebserverStringArray *&stringlist, DOM_Runtime *runtime);

	/* From DOM_DOMStringList::Generator: */
	virtual unsigned StringList_length();
	virtual OP_STATUS StringList_item(int index, const uni_char *&name);
	virtual BOOL StringList_contains(const uni_char *string);

	UINT32 GetCount() const { return m_strings.GetCount(); }
	OP_STATUS Add(OpString* item) { return m_strings.Add(item); }
	OP_STATUS Add(const uni_char* item);
	OpAutoVector<OpString> *GetStrings() { return &m_strings; }
private:
	DOM_WebserverStringArray();
	OpAutoVector<OpString> m_strings;
};

class DOM_WebServerResponse
	: public DOM_Object,
	  private Webserver_HttpConnectionListener
{
public:
	static OP_STATUS Make(DOM_WebServerResponse *&response, DOM_Runtime *runtime, WebResource_Custom *web_resource_script, DOM_WebServerConnection *connection);

	virtual ~DOM_WebServerResponse();

	/* From DOM_Object: */
	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_WEBSERVER_RESPONSE || DOM_Object::IsA(type); }
	virtual void GCTrace();

	/* From Webserver_HttpConnectionListener: */
	virtual OP_STATUS WebServerConnectionKilled();

	DOM_DECLARE_FUNCTION(setResponseHeader);
	DOM_DECLARE_FUNCTION(setStatusCode);
	DOM_DECLARE_FUNCTION(setProtocolString);
	DOM_DECLARE_FUNCTION(write);
	DOM_DECLARE_FUNCTION(writeLine);
	DOM_DECLARE_FUNCTION(writeBytes);
	DOM_DECLARE_FUNCTION(writeFile);
	DOM_DECLARE_FUNCTION(writeImage);
	DOM_DECLARE_FUNCTION(close);
	DOM_DECLARE_FUNCTION(closeAndRedispatch);
	DOM_DECLARE_FUNCTION(flush);

	enum {
		FUNCTIONS_setResponseHeader = 1,
		FUNCTIONS_setResponseCode,
		FUNCTIONS_setProtocolString,
		FUNCTIONS_write,
		FUNCTIONS_writeLine,
		FUNCTIONS_writeBytes,
		FUNCTIONS_writeFile,
		FUNCTIONS_writeImage,
		FUNCTIONS_close,
		FUNCTIONS_closeAndRedispatch,
		FUNCTIONS_flush,
		FUNCTIONS_ARRAY_SIZE
	};

	WebResource_Custom *GetWebResourceCustom() { return static_cast<WebResource_Custom *>(m_web_resource_script.Ptr()); }

private:
	DOM_WebServerResponse(WebResource_Custom *web_resource_script, DOM_WebServerConnection *connection);

	BOOL m_connection_is_alive;
	BOOL m_implicit_flush;

	DOM_WebServerConnection *m_connection;

	WebResourceWrapper m_web_resource_script;
};

class DOM_WebServerUploadedFile
	: public DOM_Object
{
public:
	static OP_STATUS Make(DOM_WebServerUploadedFile *&uploaded_file, DOM_Runtime *runtime, WebserverUploadedFile *file);

	DOM_DECLARE_FUNCTION(getFileHeader);

	enum {
		FUNCTIONS_getFileHeader = 1,
		FUNCTIONS_ARRAY_SIZE
	};

	WebserverUploadedFile *GetUploadedFilePointer() { return m_uploaded_file.Ptr();}

	/* From DOM_Object: */
	virtual ES_GetState GetName(OpAtom property_name, ES_Value* return_value, ES_Runtime* origining_runtime);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_WEBSERVER_UPLOADED_FILE || DOM_Object::IsA(type); }
	virtual void GCTrace();

private:
	DOM_WebServerUploadedFile(WebserverUploadedFile *file);

	WebserverUploadedFileWrapper m_uploaded_file;

	DOM_WebServerCollection *m_header_collection;
};

class DOM_WebServerCollection
	: public DOM_Object,
	  public AddToCollectionCallback
{
public:
	virtual ~DOM_WebServerCollection(){};

	OP_BOOLEAN GetFromCollection(const uni_char* key, DOM_WebServerObjectArray *&object_array);

	static OP_STATUS MakeHeaderCollection(DOM_WebServerCollection *&header_collection, DOM_Runtime *runtime, HeaderList *header_list);
	static OP_STATUS MakeItemCollectionUri(DOM_WebServerCollection *&item_collection, DOM_Runtime *runtime, WebResource_Custom *web_resource_script);
	static OP_STATUS MakeItemCollectionBody(DOM_WebServerCollection *&item_collection, DOM_Runtime *runtime, WebResource_Custom *web_resource_script);

	/* From DOM_Object: */
	virtual BOOL IsA(int type) { return type == DOM_TYPE_WEBSERVER_COLLECTION || DOM_Object::IsA(type); }

	/* From AddToCollectionCallback: */
	OP_STATUS AddToCollection(const uni_char* key, const uni_char* data_string);
protected:
	DOM_WebServerCollection(){};

};

class DOM_WebServerServiceDescriptor
	: public DOM_Object
{
public:
	virtual ~DOM_WebServerServiceDescriptor(){};


	static OP_STATUS Make(DOM_WebServerServiceDescriptor *&descriptor, DOM_Runtime *runtime, const uni_char* service_name, const uni_char* service_path, const uni_char* service_uri, const uni_char* service_description, const uni_char* service_author, const uni_char* service_origin_url, BOOL has_active_auth);

	/* From DOM_Object: */
	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_WEBSERVER_SERVICE_DESCRIPTOR || DOM_Object::IsA(type); }

	OP_STATUS AddIcon(DOM_WidgetIcon *icon);

	virtual void GCTrace();

private:
	DOM_WebServerServiceDescriptor(BOOL has_active_auth);

	OpString m_service_name;
	OpString m_service_path;
	OpString m_service_description;
	OpString m_service_author;
	OpString m_service_uri;
	OpString m_service_origin_url;
	BOOL m_has_active_auth;
	DOM_WebServerObjectArray *m_icons;	// List of icons
};

/** The global webserver object, which enables server side scripting. */
class DOM_WebServer
	: public DOM_Object,
	  public DOM_EventTargetOwner,
	  public ObjectFlushedCallback,
	  public FlushIsFinishedCallback,
	  public WebSubserverEventListener
{
public:
	DOM_WebServer(WebSubServer *subserver);

	virtual ~DOM_WebServer();

	static void PutConstructorsL(DOM_Object* target);

	void IncreaseNumberOfOpenedConnections(){ m_total_connection_count++;}
	unsigned int GetNumberOfOpenedConnections(){ return m_total_connection_count;}
	WebSubServer *GetSubServer(){ return m_subserver;}

	OP_STATUS AppendBodyObject(DOM_Object *dom_object, WebserverBodyObject_Base *webserver_body_object, DOM_WebServerResponse *responseObject);
	OP_STATUS Flush(WebResource_Custom *m_web_resource_script, BOOL closeConnection, unsigned int connection_id, ES_Object* flush_finished_callback = NULL);

	/* From DOM_Object: */
	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);

	virtual void InitializeL(ES_Runtime* runtime);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_WEBSERVER || DOM_Object::IsA(type); }
	virtual void GCTrace();

#ifdef DOM_GADGET_FILE_API_SUPPORT
	DOM_DECLARE_FUNCTION(shareFile);
	DOM_DECLARE_FUNCTION(unshareFile);
	DOM_DECLARE_FUNCTION(sharePath);
	DOM_DECLARE_FUNCTION(unsharePath);
	DOM_DECLARE_FUNCTION(addEventListener);
#endif // DOM_GADGET_FILE_API_SUPPORT
	DOM_DECLARE_FUNCTION(getContentType);
	DOM_DECLARE_FUNCTION(setDefaultHeader);

	/* From ObjectFlushedCallback: */
	virtual OP_STATUS BodyObjectFlushed(WebserverBodyObject_Base *bodyDataObject);

	/* From FlushIsFinishedCallback: */
	virtual OP_STATUS FlushIsFinished(unsigned int flush_id);

	/* From  WebSubserverEventListener: */
	virtual void OnWebSubServerDelete() { m_subserver = NULL; }
	virtual void OnNewRequest(WebserverRequest *){}
	virtual void OnTransferProgressIn(double, UINT, WebserverRequest*, UINT){}
	virtual void OnTransferProgressOut(double, UINT, WebserverRequest*, UINT, BOOL){}

private:
	class BodyObjectContainer
	{
	public:
		BodyObjectContainer(DOM_Object *dom_flush_object, WebserverBodyObject_Base *body_object)
			: m_dom_flush_object(dom_flush_object),
			  m_flush_object(body_object)
		{}

		DOM_Object *m_dom_flush_object;
		WebserverBodyObject_Base *m_flush_object;
	};

	class FlushCallbackContainer
	{
	public:
		FlushCallbackContainer(ES_Object *flush_finished_callback, unsigned int flush_id, unsigned int connection_id)
			: m_flush_finished_callback(flush_finished_callback),
			  m_flush_id(flush_id),
			  m_connection_id(connection_id)

		{}
		ES_Object *m_flush_finished_callback;
		unsigned int m_flush_id;
		unsigned int m_connection_id;
	};

	/* from DOM_EventTargetOwner */
	virtual DOM_Object *GetOwnerObject() { return this; }

	unsigned int m_total_connection_count;
	WebSubServer *m_subserver;
	unsigned int m_flush_counter;

	OpVector<FlushCallbackContainer> m_flush_finished_callbacks;

	OpVector<BodyObjectContainer> m_flushing_dom_objects;
};

class DOM_WebServerSession
	: public DOM_Object
{
public:
	virtual ~DOM_WebServerSession(){};

	static OP_STATUS Make(DOM_WebServerSession *&auth, DOM_Runtime *runtime, const uni_char* session_id, const uni_char* username, WebSubServerAuthType type);

	/* From DOM_Object: */
	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_WEBSERVER_SESSION || DOM_Object::IsA(type); }

	DOM_DECLARE_FUNCTION(logout);

	enum {
		FUNCTIONS_logout = 1,
		FUNCTIONS_ARRAY_SIZE
	};

private:
	DOM_WebServerSession(WebSubServerAuthType type);

	OpString m_username;
	OpString m_session_id;

	WebSubServerAuthType m_type;

};

#endif // WEBSERVER_SUPPORT

#if defined UPNP_SUPPORT && defined UPNP_SERVICE_DISCOVERY

class DOM_WebServerObjectArray
	: public DOM_Object
{
public:
	static OP_STATUS Make(DOM_WebServerObjectArray *&object_array, DOM_Runtime *runtime, const char* class_name);
#ifdef DOM_GADGET_FILE_API_SUPPORT
	static OP_STATUS MakeUploadedFilesArray(DOM_GadgetFile *&file_array, DOM_Runtime *runtime, WebResource_Custom *web_resource_script);
#endif //DOM_GADGET_FILE_API_SUPPORT
	static OP_STATUS MakeServiceArray(DOM_WebServerObjectArray *&service_array, DOM_Runtime *runtime);
	/* From DOM_Object: */
	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime* origining_runtime);
	virtual ES_GetState GetIndex(int property_index, ES_Value *value, ES_Runtime* origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_WEBSERVER_ARRAY || DOM_Object::IsA(type); }
	virtual void GCTrace();
	OP_STATUS Add(DOM_Object *object) { return m_objects.Add(object); }
	UINT32 GetCount() { return m_objects.GetCount(); }
	DOM_Object *Get(UINT32 idx) { return m_objects.Get(idx); }
	DOM_Object *Remove(UINT32 idx) { return m_objects.Remove(idx); }
private:
	OpVector<DOM_Object> m_objects;
};

#endif //UPNP_SUPPORT && UPNP_SERVICE_DISCOVERY

#endif // DOM_DOMWEBSERVER_H
