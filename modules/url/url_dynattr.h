/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve Pettersen
 */


#ifndef _URL_DYNAMIC_ATTRIBUTE_H
#define _URL_DYNAMIC_ATTRIBUTE_H

#include "modules/util/twoway.h"

/**
 *	URL_DynamicUIntAttributeHandler is the base class used to implment handlers for
 *	registered UInt Atributes in the URL API
 *	
 *	Each registered attribute must reference a object that inherits from this 
 *	class. 
 *	
 *	Objects of this class can be used directly if no special handling is needed.
 *
 *	Multiple attributes can ONLY reference the same handler object if it is not cacheable,
 *	and they MUST NOT reference any of the attributes that are using that object as a handler.
 *
 *	The Handler functions can retrieve and update information in the URL, and can thus be 
 *	used as meta attributes.
 *
 *	Workflow:
 *
 *	 1.	Application creates and configures a handler object
 *
 *	 2.	The application registers the handler with the URL module using a call
 *
 *			URL_Uint32Attribute my_new_attribute = g_url_api->RegisterAttributeL(handler);
 *
 *		The returned value stored in "my_new_attribute" is the enum value that must be used 
 *		in all calls to URL::Get/SetAttribute(). This value is dynamically allocated and may 
 *		be different between different runs.
 *
 *	 3.	Setting the attribute is done using url.SetAttribute(my_new_attribute, value);
 *
 *		Before registering the value URL will call the handler's OnSetValue function, which 
 *		may change the value, decide not to set it, or use it to set other attributes.
 *
 *	 4.	Getting the attribute value is done using url.GetAttribute(my_new_attribute);
 *		The default value is 0 (zero).
 *
 *		Before returning the value, the URL will call the handler's OnGetValue function, which 
 *		may change the value, or generate it based on other attributes of the URL.
 *
 *	 5.	If the attribute is cachable it will be stored in the cache index with the value that 
 *		is currently stored. OnGetValue WILL NOT be called during cache index writing. Similarly,
 *		OnSetValue WILL NOT be called during cache index reading.
 */
class URL_DynamicUIntAttributeHandler : public TwoWayPointer_Target
{
private:
	/** Attribute module identifier. Used as ID in cache indexes. 
	 *	MUST be unique within Opera for integer attributes, 
	 *	and not change between runs, builds, or profiles.
	 *	Also, if the value is moved to a new module, this value 
	 *  MUST be kept in the new module (the original value is kept)
	 */
	uint32 module_identifier;

	/** Attribute identifier. Used as ID in cache indexes. 
	 *	MUST be unique within a given module for integer attributes, 
	 *	and not change between runs, builds, or profiles
	 *	Also, if the value is moved to a new module, this value 
	 *  MUST be kept in the new module (the original value is kept)
	 */
	uint32 identifier;

	/** Store this in the cache index? */
	BOOL store_in_cache_index;

	/** This attribute is a flag; any non-zero input value is considered TRUE, TRUE and FALSE are the only values returned for output */
	BOOL is_flag;

public:

	/** Constructor 
	 *	@param mod_id: The unique module ID for the entry, used in the cache index value
	 *	@param id	: The unique ID of the entry, used as cache index value
	 *	@param cache: Store the attribute in the cache index file if TRUE
	 */
	URL_DynamicUIntAttributeHandler(uint32 mod_id=0, uint32 id=0, BOOL cache=FALSE): module_identifier(mod_id), identifier(id), store_in_cache_index(cache), 
			is_flag(FALSE){
			OP_ASSERT(module_identifier <= USHRT_MAX); // If we ever get to 65K modules we have a !serious! design problem
			OP_ASSERT(identifier <= USHRT_MAX); // If we ever get to 65K tags in a module we have another serious problem: feature creep.
		};

	/** Destructor */
	virtual ~URL_DynamicUIntAttributeHandler(){};

	/** This action is called each time the attribute is updated before the value is set
	 *	The value may be edited (e.g range limited), or discarded by the handler.
	 *	It is also possible to use the input value to update other attributes, 
	 *	then discard the input value. 
	 *	The implementation may retrieve other attributes of the target URL,
	 *	or set other attributes of the target URL, but MUST NOT 
	 *	do so for the attribute it is configured for.
	 *
	 *	@param	url: URL object for which the attribute is being set.
	 *				Can be used to retrieve or set other attributes.
	 *	@param	in_out_value: On entry, the value that is being set; 
	 *					On exit contains the (optionally modified) 
	 *					value to be set if set_value returns with TRUE
	 *
	 *	@param	set_value: On exit contains TRUE if the value is to be 
	 *					set for the attribute
	 *					The value of this parameter will always be FALSE on entry
	 *					The default implementation will always set this to TRUE.
	 *
	 *	@return		OP_STATUS	If successful, and the set_value is TRUE 
	 *					the value in in_out_value is set for the URL.
	 */
	virtual OP_STATUS OnSetValue(URL &url, uint32 &in_out_value, BOOL &set_value) const;

	/** This action is called each time the value of the attribute is retrieved before the value is returned
	 *	The value may be edited (e.g range limited).
	 *	It is also possible to use the values of other attributes to 
	 *	construct the value that is to be returned.
	 *	The implementation may retrieve other attributes of the target URL,
	 *	or set other attributes of the target URL, but MUST NOT 
	 *	do so for the attribute it is configured for.
	 *
	 *	@param	url: URL object for which the attribute is being set.
	 *				Can be used to retrieve or set other attributes.
	 *	@param	in_out_value: On entry, the value registered in the URL; 
	 *					On exit contains the (optionally modified) 
	 *					value to be returned to the requester
	 *
	 *	@return		OP_STATUS	Status of operation, e.g. OOM
	 */
	virtual OP_STATUS OnGetValue(URL &url, uint32 &in_out_value) const;

	/** What is the module ID? */
	uint32 GetModuleId() const{return module_identifier;}

	/** What is the tag identifier? */
	uint32 GetTagID() const{return identifier;}

	/** Is this cachable? */
	BOOL GetCachable() const{return store_in_cache_index;}

	/** Set the Is Flag attribute */
	void SetIsFlag(BOOL flag) {is_flag = flag;}

	/** Get the Is Flag attribute */
	BOOL GetIsFlag() const{return is_flag;}
};

/**
 *	URL_DynamicStringAttributeHandler is the base class used to implment handlers for
 *	registered OpString8 Attributes in the URL API
 *	
 *	Each registered attribute must reference a object that inherits from this 
 *	class. 
 *	
 *	Objects of this class can be used directly if no special handling is needed.
 *
 *	Multiple attributes can ONLY reference the same handler object if it is not cacheable,
 *	and they MUST NOT reference any of the attributes that are using that object as a handler.
 *
 *	The Handler functions can retrieve and update information in the URL, and can thus be 
 *	used as meta attributes.
 *
 *	String Attributes can be associated with a HTTP header for sending, receiving, or both.
 *	NOTE! NOTE: CARE MUST BE TAKEN IF THE IDENTIFIED HEADER IS ALSO UPDATED BY THE HTTP STACK.
 *	SOME HEADERS CANNOT BE UPDATED BY NON-HTTP CODE.
 *
 *	Workflow:
 *
 *	 1.	Application creates and configures a handler object
 *
 *	 2.	The application registers the handler with the URL module using a call
 *
 *			URL_StringAttribute my_new_attribute = g_url_api->RegisterAttributeL(handler);
 *
 *		The returned value stored in "my_new_attribute" is the enum value that must be used 
 *		in all calls to URL::Get/SetAttribute(). This value is dynamically allocated and may 
 *		be different between different runs.
 *
 *	 3.	Setting the attribute is done using url.SetAttribute(my_new_attribute, value);
 *
 *		Before registering the value URL will call the handler's OnSetValue function, which 
 *		may change the value, decide not to set it, or use it to set other attributes.
 *
 *	 4.	Getting the attribute value is done using url.GetAttribute(my_new_attribute);
 *		The default value is 0 (zero).
 *
 *		Before returning the value, the URL will call the handler's OnGetValue function, which 
 *		may change the value, or generate it based on other attributes of the URL.
 *
 *	 5.	If the attribute is cachable it will be stored in the cache index with the value that 
 *		is currently stored. OnGetValue WILL NOT be called during cache index writing. Similarly,
 *		OnSetValue WILL NOT be called during cache index reading.
 *
 *	 6. When constructing HTTP requests URL will first perform a OnGetValue call, then call 
 *		OnSendHTTPHeader in the handler. The handler may then decide whether or not to send 
 *		the header, and may construct or edit it based on other attributes. If multiple attributes
 *		are registered for the same header they are concatenated in commaseparated form
 *
 *	 7. When receiving HTTP requests URL will first perform a OnRecvHTTPHeader call to the
 *		handler, which may then decide to set it, or not, or to use the received value to
 *		construct another value or to update other attributes based on the value. 
 *		If the value is to be set, a call to OnSetValue is then made.
 */
class URL_DynamicStringAttributeHandler : public TwoWayPointer_Target
{
private:
	/** Attribute module identifier. Used as ID in cache indexes. 
	 *	MUST be unique within Opera for string attributes,  
	 *	and not change between runs, builds, or profiles
	 *	Also, if the value is moved to a new module, this value 
	 *  MUST be kept in the new module (the original value is kept)
	 */
	uint32 module_identifier;

	/** Attribute identifier. Used as ID in cache indexes. 
	 *	MUST be unique within a given module for string attributes, 
	 *	and not change between runs, builds, or profiles
	 *	Also, if the value is moved to a new module, this value 
	 *  MUST be kept in the new module (the original value is kept)
	 */
	uint32 identifier;

	/** Store this in the cache index? */
	BOOL store_in_cache_index;

#if defined URL_ENABLE_DYNATTR_SEND_HEADER || defined URL_ENABLE_DYNATTR_RECV_HEADER || defined URL_ENABLE_DYNATTR_SEND_MULTIHEADER || defined URL_ENABLE_DYNATTR_RECV_MULTIHEADER
	/** Which header HTTP header name is this attribute associated with 
	 *	If multiple attributes support the same header, results are 
	 *	comma concatenated; Order of concatenation is not predetermined.
	 */
	OpString8 http_header_name;
#endif

#if defined URL_ENABLE_DYNATTR_SEND_MULTIHEADER || defined URL_ENABLE_DYNATTR_RECV_MULTIHEADER
	/** True if the header is a prefix */
	BOOL header_name_is_prefix;
#endif

#if defined URL_ENABLE_DYNATTR_SEND_HEADER || defined URL_ENABLE_DYNATTR_SEND_MULTIHEADER
	/** Send this attribute as a HTTP header
	 */
	BOOL send_http;
#endif

#if defined URL_ENABLE_DYNATTR_RECV_HEADER || defined URL_ENABLE_DYNATTR_RECV_MULTIHEADER
	/** Set this attribute from the identified HTTP header when parsing HTTP headers.
	 */
	BOOL get_http;
#endif

public:
	/** Constructor 
	 *	@param mod_id: The unique module ID for the entry, used in the cache index value
	 *	@param id	: The unique ID of the entry, used as cache index value
	 *	@param cache: Store the attribute in the cache index file if TRUE
	 */
	URL_DynamicStringAttributeHandler(uint32 mod_id=0, uint32 id=0, BOOL cache=FALSE): 
				module_identifier(mod_id), identifier(id), store_in_cache_index(cache)
#if defined URL_ENABLE_DYNATTR_SEND_MULTIHEADER || defined URL_ENABLE_DYNATTR_RECV_MULTIHEADER
					, header_name_is_prefix(FALSE)
#endif
#if defined URL_ENABLE_DYNATTR_SEND_HEADER || defined URL_ENABLE_DYNATTR_SEND_MULTIHEADER
				, send_http(FALSE)
#endif
#if defined URL_ENABLE_DYNATTR_RECV_HEADER || defined URL_ENABLE_DYNATTR_RECV_MULTIHEADER
				, get_http(FALSE)
#endif
				{
			OP_ASSERT(module_identifier <= USHRT_MAX); // If we ever get to 65K modules we have a !serious! design problem
			OP_ASSERT(identifier <= USHRT_MAX); // If we ever get to 65K tags in a module we have another serious problem: feature creep.
		};

	/** Destructor */
	virtual ~URL_DynamicStringAttributeHandler(){};

#if defined URL_ENABLE_DYNATTR_SEND_HEADER || defined URL_ENABLE_DYNATTR_RECV_HEADER
	/**
	 *
	 *	If this attribute is to be sent or received as a HTTP header 
	 *	this must be used to define the requirements
	 *
	 *  Enabled by: API_URL_DYNATTR_SEND_HEADER or API_URL_DYNATTR_RECV_HEADER
	 *
	 *	@param	hdr_name		Name of Header; MUST be a token, format is checked and operation will fail if not of correct form
	 *	@param	send			If TRUE, send this attribute as a header
	 *	@param	recv			If TRUE, set this attribute from the given header
	 *
	 *	@return	OP_STATUS		error status
	 */
	OP_STATUS	SetHTTPInfo(const OpStringC8 &hdr_name, BOOL send, BOOL recv);
#endif

#if defined URL_ENABLE_DYNATTR_SEND_MULTIHEADER || defined URL_ENABLE_DYNATTR_RECV_MULTIHEADER
	/**
	 *	If this attribute is to be sent or received as a group of HTTP headers
	 *	matching a prefix this must be used to define the requirements.
	 *	Attributes using this is sent through
	 *
	 *  Enabled by: API_URL_DYNATTR_SEND_MULTIHEADER or API_URL_DYNATTR_RECV_MULTIHEADER
	 *
	 *	@param	hdr_name		Prefix of Headernames; MUST be a token, format is checked and operation will fail if not of correct form
	 *	@param	send			If TRUE, send this attribute as a header
	 *	@param	recv			If TRUE, set this attribute from the given header
	 *
	 *	@return	OP_STATUS		error status
	 */
	OP_STATUS	SetPrefixHTTPInfo(const OpStringC8 &hdr_name, BOOL send, BOOL recv);
#endif

	/** This action is called each time the attribute is updated before the value is set
	 *	The value may be edited (e.g range limited), or discarded by the handler.
	 *	It is also possible to use the input value to update other attributes, 
	 *	then discard the input value. 
	 *	The implementation may retrieve other attributes of the target URL,
	 *	or set other attributes of the target URL, but MUST NOT 
	 *	do so for the attribute it is configured for.
	 *
	 *	@param	url: URL object for which the attribute is being set.
	 *				Can be used to retrieve or set other attributes.
	 *	@param	in_out_value: On entry, the value that is being set; 
	 *					On exit contains the (optionally modified) 
	 *					value to be set if set_value returns with TRUE
	 *
	 *	@param	set_value: On exit contains TRUE if the value is to be 
	 *					set for the attribute
	 *					The value of this parameter will always be FALSE on entry
	 *					The default implementation will always set this to TRUE.
	 *
	 *	@return		OP_STATUS	If successful, and the set_value is TRUE 
	 *					the value in in_out_value is set for the URL.
	 */
	virtual OP_STATUS OnSetValue(URL &url, OpString8 &in_out_value, BOOL &set_value) const;

	/** This action is called each time the value of the attribute is retrieved before the value is returned
	 *	The value may be edited (e.g range limited).
	 *	It is also possible to use the values of other attributes to 
	 *	construct the value that is to be returned.
	 *	The implementation may retrieve other attributes of the target URL,
	 *	or set other attributes of the target URL, but MUST NOT 
	 *	do so for the attribute it is configured for.
	 *
	 *	@param	url: URL object for which the attribute is being set.
	 *				Can be used to retrieve or set other attributes.
	 *	@param	in_out_value: On entry, the value registered in the URL; 
	 *					On exit contains the (optionally modified) 
	 *					value to be returned to the requester
	 *
	 *	@return		OP_STATUS	Status of operation, e.g. OOM
	 */
	virtual OP_STATUS OnGetValue(URL &url, OpString8 &in_out_value) const;

#ifdef URL_ENABLE_DYNATTR_SEND_HEADER
	/** This action is called each time the identified HTTP header is constructed
	 *	for this attribute, and returns the actual header value to be added, and 
	 *	whether or it is to be sent
	 *
	 *	The value may be edited (e.g range limited), or discarded by the handler.
	 *	It is also possible to use the input value to update other attributes, 
	 *	then discard the input value. 
	 *
	 *	The implementation may retrieve other attributes of the target URL,
	 *	or set other attributes of the target URL, but MUST NOT 
	 *	do so for the attribute it is configured for.
	 *
	 *	Note: OnGetValue is called first.
	 *
	 *  Enabled by: API_URL_DYNATTR_SEND_HEADER
	 *
	 *	@param	url: URL object for which the attribute is being set.
	 *				Can be used to retrieve or set other attributes.
	 *	@param	in_out_value: On entry, the value that was registered with the URL
	 *					On exit contains the (optionally modified) 
	 *					value to be sent in the header if sebd returns with TRUE
	 *
	 *	@param	send: On exit contains TRUE if the value is to be 
	 *					sent for as a HTTP header. 
	 *					The value of this parameter will always be FALSE on entry
	 *					The default implementation will always set this to TRUE.
	 *
	 *	@return		OP_STATUS	If successful, and the send parameter is TRUE 
	 *					the value in in_out_value is added to the identified HTTP header
	 */
	virtual OP_STATUS OnSendHTTPHeader(URL &url, OpString8 &in_out_value, BOOL &send) const;
#endif

#if defined URL_ENABLE_DYNATTR_RECV_HEADER
	/** This action is called each time the identified HTTP header is received
	 *	for this attribute, and returns the actual value to be set for the attribute, and 
	 *	whether or it is to be set
	 *
	 *	The value may be edited (e.g parsed or range limited), or discarded by the handler.
	 *	It is also possible to use the input value to update other attributes, 
	 *	then discard the input value. 
	 *
	 *	The implementation may retrieve other attributes of the target URL,
	 *	or set other attributes of the target URL, but MUST NOT 
	 *	do so for the attribute it is configured for.
	 *
	 *	Note: OnSetValue is called afterwards.
	 *
	 *  Enabled by: API_URL_DYNATTR_RECV_HEADER
	 *
	 *	@param	url: URL object for which the attribute is being set.
	 *				Can be used to retrieve or set other attributes.
	 *	@param	in_out_value: On entry, the value that was received in the header
	 *					On exit contains the (optionally modified) 
	 *					value to be set in the header if set returns with TRUE
	 *
	 *	@param	set: On exit contains TRUE if the value is to be 
	 *					set as a attribute
	 *					The value of this parameter will always be FALSE on entry
	 *					The default implementation will always set this to TRUE.
	 *
	 *	@return		OP_STATUS	If successful, and the set parameter is TRUE 
	 *					the value in in_out_value is set as the identified attribute in the URL
	 */
	virtual OP_STATUS OnRecvHTTPHeader(URL &url, OpString8 &in_out_value, BOOL &set) const;
#endif

#ifdef URL_ENABLE_DYNATTR_SEND_MULTIHEADER
	/** This is called for attributes whose headernames are have the 
	 *	header name prefix specified in the attribute.
	 *
	 *	The values are a pointer to a list of OpStringC8 objects and the number of 
	 *	items in the list. The list must be owner by the handler or its owner, and must remain in 
	 *	existence while the headers for the URL is being processed.
	 *	
	 *  Enabled by: API_URL_DYNATTR_SEND_MULTIHEADER
	 *
	 *	@param	url: URL object for which the attribute is being set.
	 *				Can be used to retrieve or set other attributes.
	 *
	 *	@param	send_headers:	Pointer to a list of headernames that MUST match the prefix, 
	 *							and must be valid headernames (if they don't the entire list is discarded)
	 *	@param	count	:	The number of headers in the send_headers
	 *
	 *	@return		OP_STATUS	If successful, the send_headers array contain count items
	 */
	virtual OP_STATUS GetSendHTTPHeaders(URL &url, const char * const*&send_headers, int &count) const;

	/** This action is called each time the identified HTTP header is constructed
	 *	for this attribute, and returns the actual header value to be added, and 
	 *	whether or it is to be sent
	 *
	 *	The value may be edited (e.g range limited), or discarded by the handler.
	 *	It is also possible to use the input value to update other attributes, 
	 *	then discard the input value. 
	 *
	 *	The implementation may retrieve other attributes of the target URL,
	 *	or set other attributes of the target URL, but MUST NOT 
	 *	do so for the attribute it is configured for.
	 *
	 *	Note: OnGetValue is called first.
	 *
	 *  Enabled by: API_URL_DYNATTR_SEND_MULTIHEADER
	 *
	 *	@param	url: URL object for which the attribute is being set.
	 *				Can be used to retrieve or set other attributes.
	 *  @param	header:	A header matching the prefix to send, in_out_value will contain 
	 *					the resulting header value that will be sent to the server.
	 *	@param	in_out_value: On entry, the value that was registered with the URL
	 *					On exit contains the (optionally modified) 
	 *					value to be sent in the header if sebd returns with TRUE
	 *
	 *	@param	send: On exit contains TRUE if the value is to be 
	 *					sent for as a HTTP header. 
	 *					The value of this parameter will always be FALSE on entry
	 *					The default implementation will always set this to TRUE.
	 *
	 *	@return		OP_STATUS	If successful, and the send parameter is TRUE 
	 *					the value in in_out_value is added to the identified HTTP header
	 */
	virtual OP_STATUS OnSendPrefixHTTPHeader(URL &url, const OpStringC8 &header, OpString8 &in_out_value, BOOL &send) const;
#endif

#ifdef URL_ENABLE_DYNATTR_RECV_MULTIHEADER
	/** This action is called each time the identified HTTP header is received
	 *	for this attribute, and returns the actual value to be set for the attribute, and 
	 *	whether or it is to be set
	 *
	 *	NOTE: As opposed to OnRecvHTTPHeader, the values of each header is NOT concatenated,
	 *	but handled individully.
	 *
	 *	The value may be edited (e.g parsed or range limited), or discarded by the handler.
	 *	It is also possible to use the input value to update other attributes, 
	 *	then discard the input value. 
	 *
	 *	The implementation may retrieve other attributes of the target URL,
	 *	or set other attributes of the target URL, but MUST NOT 
	 *	do so for the attribute it is configured for.
	 *
	 *	Note: OnSetValue is called afterwards.
	 *
	 *  Enabled by: API_URL_DYNATTR_RECV_MULTIHEADER
	 *
	 *	@param	url: URL object for which the attribute is being set.
	 *				Can be used to retrieve or set other attributes.
	 *	@param	headername:	The name of the header currently being process, 
						prefix matches the handler's header prefix.
	 *	@param	in_out_value: On entry, the value that was received in the header
	 *					On exit contains the (optionally modified) 
	 *					value to be set in the header if set returns with TRUE
	 *
	 *	@param	set: On exit contains TRUE if the value is to be 
	 *					set as a attribute
	 *					The value of this parameter will always be FALSE on entry
	 *					The default implementation will always set this to TRUE.
	 *
	 *	@return		OP_STATUS	If successful, and the set parameter is TRUE 
	 *					the value in in_out_value is set as the identified attribute in the URL
	 */
	virtual OP_STATUS OnRecvPrefixHTTPHeader(URL &url, const OpStringC8 &headername, OpString8 &in_out_value, BOOL &set) const;
#endif

	/** What is the module ID? */
	uint32 GetModuleId() const{return module_identifier;}

	/** What is the tag identifier? */
	uint32 GetTagID() const{return identifier;}

	/** Is this cachable? */
	BOOL GetCachable() const{return store_in_cache_index;}

#if defined URL_ENABLE_DYNATTR_SEND_HEADER || defined URL_ENABLE_DYNATTR_RECV_HEADER || defined URL_ENABLE_DYNATTR_SEND_MULTIHEADER || defined URL_ENABLE_DYNATTR_RECV_MULTIHEADER
	/** What is the HTTP header name for this attribute */
	const OpStringC8 &GetHTTPHeaderName() const{return http_header_name;}
#endif

#if defined URL_ENABLE_DYNATTR_SEND_MULTIHEADER || defined URL_ENABLE_DYNATTR_RECV_MULTIHEADER
	/** Is this header name a prefix of a group of headers? */
	BOOL GetIsPrefixHeader() const{return header_name_is_prefix;}
#endif

#if defined URL_ENABLE_DYNATTR_SEND_HEADER || defined URL_ENABLE_DYNATTR_SEND_MULTIHEADER
	/** What is the HTTP header name for this attribute */
	BOOL GetSendHTTP() const{return send_http;}
#endif

#if defined URL_ENABLE_DYNATTR_RECV_HEADER || defined URL_ENABLE_DYNATTR_RECV_MULTIHEADER
	/** What is the HTTP header name for this attribute */
	BOOL GetReceiveHTTP() const{return get_http;}
#endif
};

/**
 *	URL_DynamicURLAttributeHandler is the base class used to implment handlers for
 *	registered Attributes handling URL attributes in the URL API
 *	
 *	Each registered attribute must reference a object that inherits from this 
 *	class. 
 *	
 *	Objects of this class can be used directly if no special handling is needed.
 *
 *	Multiple attributes can ONLY reference the same handler object if it is not cacheable,
 *	and they MUST NOT reference any of the attributes that are using that object as a handler.
 *
 *	The Handler functions can retrieve and update information in the URL, and can thus be 
 *	used as meta attributes.
 *
 *	Workflow:
 *
 *	 1.	Application creates and configures a handler object
 *
 *	 2.	The application registers the handler with the URL module using a call
 *
 *			URL_URLAttribute my_new_attribute = g_url_api->RegisterAttributeL(handler);
 *
 *		The returned value stored in "my_new_attribute" is the enum value that must be used 
 *		in all calls to URL::Get/SetAttribute(). This value is dynamically allocated and may 
 *		be different between different runs.
 *
 *	 3.	Setting the attribute is done using url.SetAttribute(my_new_attribute, value);
 *
 *		Before registering the value URL will call the handler's OnSetValue function, which 
 *		may change the value, decide not to set it, or use it to set other attributes.
 *
 *	 4.	Getting the attribute value is done using url.GetAttribute(my_new_attribute);
 *		The default value is 0 (zero).
 *
 *		Before returning the value, the URL will call the handler's OnGetValue function, which 
 *		may change the value, or generate it based on other attributes of the URL.
 *
 *	 5.	If the attribute is cachable it will be stored in the cache index with the 7-bit string 
 *		representation value that is currently stored. OnGetValue WILL NOT be called during 
 *		cache index writing. Similarly,
 *		OnSetValue WILL NOT be called during cache index reading.
 */
class URL_DynamicURLAttributeHandler : public TwoWayPointer_Target
{
private:
	/** Attribute module identifier. Used as ID in cache indexes. 
	 *	MUST be unique within Opera for integer attributes, 
	 *	and not change between runs, builds, or profiles.
	 *	Also, if the value is moved to a new module, this value 
	 *  MUST be kept in the new module (the original value is kept)
	 */
	uint32 module_identifier;

	/** Attribute identifier. Used as ID in cache indexes. 
	 *	MUST be unique within a given module for integer attributes, 
	 *	and not change between runs, builds, or profiles
	 *	Also, if the value is moved to a new module, this value 
	 *  MUST be kept in the new module (the original value is kept)
	 */
	uint32 identifier;

	/** Store this in the cache index? */
	BOOL store_in_cache_index;

public:

	/** Constructor 
	 *	@param mod_id: The unique module ID for the entry, used in the cache index value
	 *	@param id	: The unique ID of the entry, used as cache index value
	 *	@param cache: Store the attribute in the cache index file if TRUE
	 */
	URL_DynamicURLAttributeHandler(uint32 mod_id=0, uint32 id=0, BOOL cache=FALSE): module_identifier(mod_id), identifier(id), store_in_cache_index(cache){
			OP_ASSERT(module_identifier <= USHRT_MAX); // If we ever get to 65K modules we have a !serious! design problem
			OP_ASSERT(identifier <= USHRT_MAX); // If we ever get to 65K tags in a module we have another serious problem: feature creep.
		};

	/** Destructor */
	virtual ~URL_DynamicURLAttributeHandler(){};

	/** This action is called each time the attribute is updated before the value is set
	 *	The value may be edited (e.g range limited), or discarded by the handler.
	 *	It is also possible to use the input value to update other attributes, 
	 *	then discard the input value. 
	 *	The implementation may retrieve other attributes of the target URL,
	 *	or set other attributes of the target URL, but MUST NOT 
	 *	do so for the attribute it is configured for.
	 *
	 *	@param	url: URL object for which the attribute is being set.
	 *				Can be used to retrieve or set other attributes.
	 *	@param	in_out_value: On entry, the value that is being set; 
	 *					On exit contains the (optionally modified) 
	 *					value to be set if set_value returns with TRUE
	 *
	 *	@param	set_value: On exit contains TRUE if the URL object is to be 
	 *					set for the attribute
	 *					The value of this parameter will always be FALSE on entry
	 *					The default implementation will always set this to TRUE.
	 *
	 *	@return		OP_STATUS	If successful, and the set_value is TRUE 
	 *					the value in in_out_value is set for the URL.
	 */
	virtual OP_STATUS OnSetValue(URL &url, URL &in_out_value, BOOL &set_value) const;

	/** This action is called each time the value of the attribute is retrieved before the value is returned
	 *	The value may be edited (e.g range limited).
	 *	It is also possible to use the values of other attributes to 
	 *	construct the value that is to be returned.
	 *	The implementation may retrieve other attributes of the target URL,
	 *	or set other attributes of the target URL, but MUST NOT 
	 *	do so for the attribute it is configured for.
	 *
	 *	@param	url: URL object for which the attribute is being set.
	 *				Can be used to retrieve or set other attributes.
	 *	@param	in_out_value: On entry, the URL object registered in the URL; 
	 *					On exit contains the (optionally modified) 
	 *					URL object to be returned to the requester
	 *
	 *	@return		OP_STATUS	Status of operation, e.g. OOM
	 */
	virtual OP_STATUS OnGetValue(URL &url, URL &in_out_value) const;

	/** What is the module ID? */
	uint32 GetModuleId() const{return module_identifier;}

	/** What is the tag identifier? */
	uint32 GetTagID() const{return identifier;}

	/** Is this cachable? */
	BOOL GetCachable() const{return store_in_cache_index;}
};



#endif // _URL_DYNAMIC_ATTRIBUTE_H
