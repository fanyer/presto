/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4;  -*-
**
** Copyright (C) 2002-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/
#ifdef UPNP_SUPPORT

#ifndef UPNP_SOAP_H_
#define UPNP_SOAP_H_

// Header of each SOAP message
#define SOAP_HEADER UNI_L("<?xml version=\"1.0\"?><s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body>")
// Schema of the SOAP envolepo, to put in the "MAN" header
#define SOAP_ENVELOPE_SCHEMA "http://schemas.xmlsoap.org/soap/envelope/"
// Footer of each SOAP message
#define SOAP_FOOTER UNI_L("</s:Body></s:Envelope>")

#ifdef _DEBUG
	#define MAX_XML_LOG_SIZE 1000000
#else
	#define MAX_XML_LOG_SIZE 16384
#endif

class UPnP;
class UPnPDevice;
class UPnPLogic;

// Class used to mange a Name/Value pair
class NamedValue
{
protected:
	OpString name;		// Tag you are looking for
	OpString value;		// Tag value
	
public:
	NamedValue() {}
	OP_STATUS Construct(const uni_char *n, const uni_char *v) { RETURN_IF_ERROR(name.Set(n)); RETURN_IF_ERROR(value.Set(v)); return OpStatus::OK; }
	OpString *GetName() { return &name; }
	OpString *GetValue() { return &value; }
	BOOL IsValueEmpty() { return value.IsEmpty(); }
	BOOL IsNameEmpty() { return value.IsEmpty(); }
	
	OP_STATUS SetName(const uni_char *str) { return name.Set(str); }
	OP_STATUS SetValue(const uni_char *str) { return value.Set(str); }
	OP_STATUS SetName(const char *str) { return name.Set(str); }
	OP_STATUS SetValue(const char *str) { return value.Set(str); }
	
	void SetEmptyValue() { value.Empty(); }
	OP_STATUS AppendValue(const uni_char *str) { return value.Append(str); }
	OP_STATUS AppendValue(const uni_char *str, int len) { return value.Append(str, len); }
};

// Manage the writing of an HTTP message
class HTTPMessageWriter
{
protected:
    UINT16 httpPort;               // Port of the HTTP server
    OpString page;				// Page URL to call (w/o 'http://' and w/o the host address)
    OpString host;				// HOST Address
    OpString content_type;      // Optional Content Type
    OpString charset;           // Optional CahrSet
    OpString soap_action;       // Optional soap action
    OpString soap_action_ns;    // Optional soap action namespace
    OpString callback;			// Optional UPnP Callback URL
    OpString nt;				// Optional UPnP Notification Type
    OpString mandatory;			// Optional "Mandatory" extension (see RFC 2774 "An HTTP Extension Framework"); at the moment, only one Mandatory extension supported per message
    OpString mandatory_ns;		// Optional "Mandatory namespace" (see RFC 2774 "An HTTP Extension Framework")
    OpString optional;			// Optional "Optional" extension (see RFC 2774 "An HTTP Extension Framework")
    OpString optional_ns;		// Optional "Optional namespace" (see RFC 2774 "An HTTP Extension Framework"); at the moment, only one Optional extension supported per message
    unsigned short timeout;      // Timeout (for UPnP it is in seconds)
    
    // Write an HTTP header, with an OpString value
    OP_STATUS WriteHeader(OpString *out_message, const char *header, OpString *value);
    // Write an HTTP header, with a uni_char* value
    OP_STATUS WriteHeader(OpString *out_message, const char *header, const uni_char *value);
    // Write an HTTP header, with an int value
    OP_STATUS WriteHeader(OpString *out_message, const char *header, long value);
    // Write an HTTP header, with an OpString value and a namespace
    OP_STATUS WriteHeaderNS(OpString *out_message, const char *header, OpString *value, OpString *ns);
    
public:
    HTTPMessageWriter() { httpPort=80; timeout=0; }
    
    // Set the Content-Type header, with the Charset
    OP_STATUS setContentType(const uni_char *contentType, const uni_char *charSet) { RETURN_IF_ERROR(content_type.Set(contentType)); RETURN_IF_ERROR(charset.Set(charSet)); return OpStatus::OK; }
    // Set the SOAP action
    OP_STATUS setSoapAction(const uni_char* service_type, const uni_char *action, const uni_char *ns);
    // Set the URL of the required resource
    OP_STATUS setURL(const uni_char *hostAddress, const uni_char *pageURL, UINT16 port=80);
    // Set the CALLBACK header
    OP_STATUS setCallback(const uni_char* callback_url) { return callback.Set(callback_url); }
    // Set the NT header
    OP_STATUS setNotificationType(const uni_char* notification_type) { return nt.Set(notification_type); }
    // Set the MAN header
    OP_STATUS setMandatory(const uni_char* man, const uni_char* ns) { RETURN_IF_ERROR(mandatory.Set(man)); return mandatory_ns.Set(ns); }
    // Set the OPT header
    OP_STATUS setOptional(const uni_char* opt, const uni_char* ns) { RETURN_IF_ERROR(optional.Set(opt)); return optional_ns.Set(ns); }
    // Set the TIMEOUT header
    void setTimeout(unsigned short seconds) { timeout=seconds; }
    
    // Compute the HTTP message and return it on the out_message variable; the http_action is usually "GET" or "POST"
    OP_STATUS GetMessage(OpString *out_message, const char * http_action, const OpString *body);
    // Return TRUE if the specified URL starts with http:// or https://
    static BOOL URLStartWithHTTP(const uni_char *url);
};
 
// Class used to generate a SOAP message
class SOAPMessage
{
public:
    // Get the SOAP Message required for calling a method (max 3 parameters allowed
    static OP_STATUS GetActionCallXML(OpString &soap_mex, const uni_char *actionName, const uni_char *serviceType, const uni_char *prefix, UINT32 num_params, ...);
    // Costruct the XML to call the action
	static OP_STATUS CostructMethod(OpString &soap_mex, const uni_char *actionName, const uni_char *serviceType, const uni_char *prefix, OpAutoVector<NamedValue> *params);
};

// Base class for UPnP Token Handlers
class Simple_XML_Handler: public XMLTokenHandler
{
	friend class UPnP;
	
    // Class used for simplify the XML parsing
    // - can keep the state of several tags
    // - cannot manage nested children tag (it suppose the tags you are looking for don't have children)
    // - The value of the tags is in the text, not in attributes
    class TagsParser
    {
    private:
		// Class used to store the state of a tag
		class TagValue: public NamedValue
		{
		protected:
			BOOL opened;		// For each element, TRUE if the tag is opened
			BOOL read;
			OpString *dest;		// Optional destination of the tag
			BOOL reset;
	
		public:
			TagValue() { opened=FALSE; read=FALSE; dest=NULL; reset=FALSE; }
			
			BOOL IsOpened() { return opened; }
			BOOL IsRead() { return read; }
			OpString *GetDest() { return dest; }
			void SetOpened(BOOL b) { opened=b; }
			void SetRead(BOOL b) { read=b; }
			void Reset() { opened=FALSE; read=FALSE; SetEmptyValue(); }
			void AutoReset() { opened=FALSE; read=FALSE; SetEmptyValue(); }
			void SetDest(OpString *str) { dest=str; }
			void SetReset(BOOL b) { reset=b; }	
		};
    
        OpString father;					// Father Tag to parse (will look for tags only if the father is null or his tag is opened)
		OpAutoVector<TagValue> tags;		// Elements to parse
		BOOL open_father;					// TRUE if the father is opened
		Simple_XML_Handler *uxh_father;		// Simple_XML_Handler object that called the handler
		UINT32 required_tags;				// Number of required tags
		
	protected:
		friend class Simple_XML_Handler;
		OpString xml_read_debug;			// XML read, for debugging purposes; it is not the real XML, it's a subset; at the time, it only support tags, no attributes at all!
		
	public:
	    TagsParser(Simple_XML_Handler *uxh);
	    // Build the TagsParser; for each parameter, you need to specify the name and a OpString * (if != NULL, will keep the value required)
	    // if reset<0, it is equivalent to say that every tag must be reset
	    OP_STATUS Construct(const uni_char *father_tag, UINT32 required, UINT32 optional, int reset, ...);
		
		OpString * getTagName(UINT32 index) { OP_ASSERT(index<GetCountAll()); if(index>=GetCountAll()) return NULL; return tags.Get(index)->GetName(); }
		OpString * getTagValue(UINT32 index) { OP_ASSERT(index<GetCountAll()); if(index>=GetCountAll()) return NULL; return tags.Get(index)->GetValue(); }
		BOOL IsRead(UINT32 index) { OP_ASSERT(index<GetCountAll()); if(index>=GetCountAll()) return FALSE; return tags.Get(index)->IsRead(); }
		OpString *GetDest(UINT32 index) { OP_ASSERT(index<GetCountAll()); if(index>=GetCountAll()) return NULL; return tags.Get(index)->GetDest(); }
		// Return the number of parameters (required+optional)
		UINT32 GetCountAll() { return tags.GetCount(); }
		// Return the number of required parameters
		UINT32 GetCountRequired() { return required_tags; }
		// True if every value has been read
		BOOL AllValuesRead();
		// Get a partial reconstruction of the parsed XML
		const uni_char *GetPartialXMLRead() { return xml_read_debug.CStr(); }
		
		// Processing help for the XMLTokenHandler
		XMLTokenHandler::Result HandleToken(XMLToken &token);
		
		// After this call, the parser can be reused; don't call him before finishing the parsing phase!
		OP_STATUS Reset();
    };
    
public:
    Simple_XML_Handler():tags_parser(this), mh(NULL), trust_http_on_fault(FALSE),upnp(NULL) { notify_missing_variables=TRUE; num_handler_calls=-1; xml_log_max_size=0; xml_parser=NULL; }
    virtual ~Simple_XML_Handler() { OP_DELETE(xml_parser); }
    
    MessageHandler *GetMessageHandler() { return &mh; }
    virtual OP_STATUS Construct() { return OpStatus::OK; }
    virtual const uni_char *GetActionName()=0;
    OpAutoVector<NamedValue> *getParameters() { return &params; }
    // Called when the parsing is finished
	virtual OP_STATUS Finished() { return OpStatus::OK; }
	// Called when there was an error in the parser. Parsing Error is responsible to call NotifyFailure()
	virtual void ParsingError(XMLParser *parser, UINT32 http_status, const char *err_mex) { NotifyFailure(err_mex); }
	// Called after ParsingError(), usually to notify the father about the failure (simplify code reuse)
	virtual OP_STATUS NotifyFailure(const char *mex) { return OpStatus::OK; };
	// Called when the parser was successful
	virtual void ParsingSuccess(XMLParser *parser) { }
	// Ges the description of the handler, for debugging purposues
	virtual const char* GetDescription()=0;
	// Return the max size allowed for the XML log size
	int GetXMLMaxLogSize() { return xml_log_max_size; }
	// Empty the partial XML used for logging
	void EmptyPartianXML() { tags_parser.xml_read_debug.Empty(); }
	// True if the error can be recovered
	virtual BOOL ErrorRecovered(XMLParser *parser, const char *error_description, const char *error_uri, const char *error_fragment, OpString &errMex) { return FALSE; }
	// Generate a standard error report for XML parsing errors
	static OP_STATUS GenerateErrorReport(XMLParser *parser, OpString *xml_err);
	// Set the parser pointer
	void SetParser(XMLParser *parser) { xml_parser=parser; }
    
protected:
	TagsParser tags_parser;				// Used to simplify the parsing
	OpAutoVector<NamedValue> params;	// Parameters for the function call
	MessageHandler mh;					// Message handler
	BOOL notify_missing_variables;		// True to notify as a failure that some variable is missing
	BOOL trust_http_on_fault;			// If there is an error during XML read, just trust the HTTP status code (if 200, then ok);
	XMLParser *xml_parser;				// PArser
	                                    // It makes sense only for actions call, that only need to say if they were successfull or not
	int num_handler_calls;				// Number of calls made to HandleTokenAuto() (used by UPnPXH_Auto)
	int xml_log_max_size;				// Max size allowed for the XML logging (used for volunteers testing)
	UPnP *upnp;							// UPnP object
    
    void ParsingStopped(XMLParser *parser);
 };

// Used to semplify the parsing in the most common cases, when you have to wait for all the values. To use this class,
// usually you need to extend it and implement HandleTokenAuto().
class UPnPXH_Auto: public Simple_XML_Handler
{
protected:
	BOOL notify_failure;			// True to notify the failure to the father
	BOOL enable_tags_subset;		// True if the notification has to happen even with someone of the tags is missing
	ReferenceUserSingle<UPnPDevice> xh_device_ref;	// Device that we are trying to use
	ReferenceUserSingle<UPnPLogic> logic_ref; // Logic that is using this object 
	ReferenceObjectSingle<XMLParser> ref_obj;  // References object
	
	void ParsingStopped(XMLParser *parser);
	virtual OP_STATUS ManageHandlerCalls();
	UPnPLogic *GetLogic() { return logic_ref.GetPointer(); }
	
	
public:
	UPnPXH_Auto(): xh_device_ref(FALSE, FALSE), logic_ref(FALSE, FALSE), ref_obj(NULL) { notify_failure=FALSE; num_handler_calls=0; enable_tags_subset=FALSE; upnp=NULL; }
	virtual ~UPnPXH_Auto() { /*OP_DELETE(xh_device);*/ }
	UPnP *GetUPnP() { return upnp; }
	
	virtual OP_STATUS Construct() { OP_ASSERT(upnp); if(!upnp) return OpStatus::ERR_NULL_POINTER; return OpStatus::OK; }
    void SetUPnP(UPnP *upnp_father, UPnPDevice *upnp_device, UPnPLogic *upnp_logic);
	XMLTokenHandler::Result HandleToken(XMLToken &token);
	// Notify a failure to the UPnP object
	virtual OP_STATUS NotifyFailure(const char *mex);
	// Function called when all the parameters have been read from the XML and upnp is not NULL. This function can be called multiple times
	virtual OP_STATUS HandleTokenAuto() { return OpStatus::OK; }
	// Return the current device
	UPnPDevice *GetDevice() { return xh_device_ref.GetPointer(); }
	BOOL ErrorRecovered(XMLParser *parser, const char *error_description, const char *error_uri, const char *error_fragment, OpString &errMex);
	/// Return the reference object
	ReferenceObject<XMLParser>* GetReferenceObjectPtr() { return &ref_obj; }
};

#define _el_dbg_(mex) dbg(mex)

#endif
#endif
