/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve Pettersen
 */


#ifndef _URL_DYNAMIC_ATTRIBUTE_INT_H
#define _URL_DYNAMIC_ATTRIBUTE_INT_H

#include "modules/url/url_dynattr.h"

class URL_DynamicAttributeManager;
class URL_DataStorage;

/** The descriptor referenced from the URL class
 *	Duplicates the information the handler, and stores
 *	the attribute ID for this object
 */
class URL_DynamicUIntAttributeDescriptor : public Link
{
	friend class URL_DynamicAttributeManager;
private:
	/** The Attribute enum ID of this attribute, used in calls to URL::GetAttribute */
	URL::URL_Uint32Attribute	attribute_id;

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

	/** Applicationprovided handler that decides what to do about the value */
	TwoWayPointer<URL_DynamicUIntAttributeHandler> handler;

	/** Is this a Flag attribute? If so, we store in a bitfield attribute*/
	BOOL is_flag;

	/** If flag, then this is the mask of the bit used in the bitfield attribute */
	uint32 flag_mask;

	/** Attribute ID of the flag_attribute */
	URL::URL_Uint32Attribute	flag_attribute_id;

public:

	/** Constructor */
	URL_DynamicUIntAttributeDescriptor(URL::URL_Uint32Attribute attr, uint32 module_id, uint32 tag_id, BOOL cachable) :
								attribute_id(attr), module_identifier(module_id), identifier(tag_id), store_in_cache_index(cachable),
								is_flag(FALSE), flag_mask(0), flag_attribute_id(URL::KNoInt)
							{
							}

	/** Construct from handler */
	OP_STATUS Construct(URL_DynamicUIntAttributeHandler *hndlr);

	/** Enable this attribute as a flag */
	OP_STATUS SetIsFlag(BOOL is_flg);

	/** Is this a flag attribute */
	BOOL GetIsFlag() const {return is_flag;}

	/** Get The flag mask used for storing this attribute */
	uint32 GetFlagMask() const {return flag_mask;}

	/** Get the Atribute id used to store the flag bit */
	URL::URL_Uint32Attribute GetFlagAttributeId() const {return flag_attribute_id;}


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
	 *	@param	url_ds: URL_DataStorage object to update for flags.
	 *					This is used to avoid URL_DataStorage intializtion race conditions
	 *	@param	in_out_value: On entry, the value that is being set; 
	 *					On exit contains the (optionally modified) 
	 *					value to be set if set_value returns with TRUE
	 *
	 *	@param	set_value: On exit contains TRUE if the value is to be 
	 *					set for the attribute
	 *
	 *	@return		OP_STATUS	If successful, and the set_value is TRUE 
	 *					the value in in_out_value is set for the URL.
	 */
	OP_STATUS OnSetValue(URL &url, URL_DataStorage *url_ds, uint32 &in_out_value, BOOL &set_value) const;

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
	OP_STATUS OnGetValue(URL &url, uint32 &in_out_value) const;

	/** Which attribute is this associated with */
	URL::URL_Uint32Attribute GetAttributeID() const{return attribute_id;}

	/** What is the module ID? */
	uint32 GetModuleId() const{return module_identifier;}

	/** What is the tag identifier? */
	uint32 GetTagID() const{return identifier;}

	/** Is this cachable? */
	BOOL GetCachable() const{return store_in_cache_index;}

	URL_DynamicUIntAttributeDescriptor *Suc(){return (URL_DynamicUIntAttributeDescriptor *) Link::Suc();}
};

/** The descriptor referenced from the URL class
 *	Duplicates the information the handler, and stores
 *	the attribute ID for this object
 */
class URL_DynamicStringAttributeDescriptor : public Link
{
	friend class URL_DynamicAttributeManager;
private:
	/** The Attribute enum ID of this attribute, used in calls to URL::GetAttribute */
	URL::URL_StringAttribute	attribute_id;

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

#if defined URL_ENABLE_DYNATTR_SEND_HEADER || defined URL_ENABLE_DYNATTR_RECV_HEADER || defined URL_ENABLE_DYNATTR_SEND_MULTIHEADER
	/** Which header HTTP header name is this attribute associated with 
	 *	If multiple attributes support the same header, results are 
	 *	comma concatenated; Order of concatenation is not predetermined.
	 */
	OpString8 http_header_name;
#endif

#if defined URL_ENABLE_DYNATTR_RECV_MULTIHEADER || defined URL_ENABLE_DYNATTR_SEND_MULTIHEADER
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

	/** Applicationprovided handler that decides what to do about the value */
	TwoWayPointer<URL_DynamicStringAttributeHandler> handler;

public:
	/** Constructor  */
	URL_DynamicStringAttributeDescriptor(URL::URL_StringAttribute attr, uint32 module_id, uint32 tag_id, BOOL cachable) :
								attribute_id(attr), module_identifier(module_id), identifier(tag_id), store_in_cache_index(cachable)
#if defined URL_ENABLE_DYNATTR_RECV_MULTIHEADER || defined URL_ENABLE_DYNATTR_SEND_MULTIHEADER
									, header_name_is_prefix(FALSE)
#endif
#if defined URL_ENABLE_DYNATTR_SEND_HEADER || defined URL_ENABLE_DYNATTR_SEND_MULTIHEADER
									, send_http(FALSE)
#endif
#if defined URL_ENABLE_DYNATTR_RECV_HEADER || defined URL_ENABLE_DYNATTR_RECV_MULTIHEADER
									, get_http(FALSE)
#endif
							{
							}

	/** Constructs the decriptor from the handler; returns error in case of OOM */
	OP_STATUS Construct(URL_DynamicStringAttributeHandler *hndlr);

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
	 *
	 *	@return		OP_STATUS	If successful, and the set_value is TRUE 
	 *					the value in in_out_value is set for the URL.
	 */
	OP_STATUS OnSetValue(URL &url, OpString8 &in_out_value, BOOL &set_value) const {
				set_value = FALSE; 
				if(handler.get() != NULL)
					return handler->OnSetValue(url, in_out_value, set_value);
				return OpStatus::OK;
			}

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
	OP_STATUS OnGetValue(URL &url, OpString8 &in_out_value) const {
				if(handler.get() != NULL)
					return handler->OnGetValue(url, in_out_value);
				return OpStatus::OK;
			}

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
	 *	@param	url: URL object for which the attribute is being set.
	 *				Can be used to retrieve or set other attributes.
	 *	@param	in_out_value: On entry, the value that was registered with the URL
	 *					On exit contains the (optionally modified) 
	 *					value to be sent in the header if send returns with TRUE
	 *
	 *	@param	send: On exit contains TRUE if the value is to be 
	 *					sent for as a HTTP header. 
	 *
	 *	@return		OP_STATUS	If successful, and the send parameter is TRUE 
	 *					the value in in_out_value is added to the identified HTTP header
	 */
	OP_STATUS OnSendHTTPHeader(URL &url, OpString8 &in_out_value, BOOL &send) const;
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
	 *	@param	url: URL object for which the attribute is being set.
	 *				Can be used to retrieve or set other attributes.
	 *	@param	in_out_value: On entry, the value that was received in the header
	 *					On exit contains the (optionally modified) 
	 *					value to be set in the header if set returns with TRUE
	 *
	 *	@param	send: On exit contains TRUE if the value is to be 
	 *					sent for as a HTTP header
	 *
	 *	@return		OP_STATUS	If successful, and the set parameter is TRUE 
	 *					the value in in_out_value is set as the identified attribute in the URL
	 */
	OP_STATUS OnRecvHTTPHeader(URL &url, OpString8 &in_out_value, BOOL &set) const {
				set = FALSE; 
				if(handler.get() != NULL)
					return handler->OnRecvHTTPHeader(url, in_out_value, set);
				return OpStatus::OK;
			}
#endif

#ifdef URL_ENABLE_DYNATTR_SEND_MULTIHEADER
	/** This is called for attributes whose headernames are have the 
	 *	header name prefix specified in the attribute.
	 *
	 *	The values are a pointer to a list of OpStringC8 objects and the number of 
	 *	items in the list. The list must be owner by the handler or its owner, and must remain in 
	 *	existence while the headers for the URL is being processed.
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
	virtual OP_STATUS OnRecvPrefixHTTPHeader(URL &url, const OpStringC8 &headername, OpString8 &in_out_value, BOOL &set) const {
				set = FALSE; 
				if(handler.get() != NULL)
					return handler->OnRecvPrefixHTTPHeader(url, headername, in_out_value, set);
				return OpStatus::OK;
			}
#endif

	/** Which attribute is this associated with */
	URL::URL_StringAttribute GetAttributeID() const{return attribute_id;}

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

	URL_DynamicStringAttributeDescriptor *Suc(){return (URL_DynamicStringAttributeDescriptor *) Link::Suc();}
};

/** The descriptor referenced from the URL class
 *	Duplicates the information the handler, and stores
 *	the attribute ID for this object
 */
class URL_DynamicURLAttributeDescriptor : public Link
{
	friend class URL_DynamicAttributeManager;
private:
	/** The Attribute enum ID of this attribute, used in calls to URL::GetAttribute */
	URL::URL_URLAttribute	attribute_id;

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

	/** Applicationprovided handler that decides what to do about the value */
	TwoWayPointer<URL_DynamicURLAttributeHandler> handler;

public:

	/** Constructor */
	URL_DynamicURLAttributeDescriptor(URL::URL_URLAttribute attr, uint32 module_id, uint32 tag_id, BOOL cachable) :
								attribute_id(attr), module_identifier(module_id), identifier(tag_id), store_in_cache_index(cachable)
							{
							}

	/** Construct from handler */
	OP_STATUS Construct(URL_DynamicURLAttributeHandler *hndlr){
		handler = hndlr; 
		if(handler.get() != NULL)
		{
			OP_ASSERT(handler->GetModuleId() <= USHRT_MAX); // If we ever get to 65K modules we have a !serious! design problem
			OP_ASSERT(handler->GetTagID() <= USHRT_MAX); // If we ever get to 65K tags in a module we have another serious problem: feature creep.

			if(handler->GetModuleId() > USHRT_MAX || handler->GetTagID() > USHRT_MAX)
				return OpStatus::ERR_OUT_OF_RANGE;

			store_in_cache_index = hndlr->GetCachable(); 
		}
		return OpStatus::OK;
	}

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
	 *
	 *	@return		OP_STATUS	If successful, and the set_value is TRUE 
	 *					the value in in_out_value is set for the URL.
	 */
	OP_STATUS OnSetValue(URL &url, URL &in_out_value, BOOL &set_value) const {
				set_value = FALSE; 
				if(handler.get() != NULL)
					return handler->OnSetValue(url, in_out_value, set_value);
				return OpStatus::OK;
			}

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
	OP_STATUS OnGetValue(URL &url, URL &in_out_value) const {
				if(handler.get() != NULL)
					return handler->OnGetValue(url, in_out_value);
				return OpStatus::OK;
			}

	/** Which attribute is this associated with */
	URL::URL_URLAttribute GetAttributeID() const{return attribute_id;}

	/** What is the module ID? */
	uint32 GetModuleId() const{return module_identifier;}

	/** What is the tag identifier? */
	uint32 GetTagID() const{return identifier;}

	/** Is this cachable? */
	BOOL GetCachable() const{return store_in_cache_index;}

	URL_DynamicURLAttributeDescriptor *Suc(){return (URL_DynamicURLAttributeDescriptor *) Link::Suc();}
};


#endif // _URL_DYNAMIC_ATTRIBUTE_INT_H
