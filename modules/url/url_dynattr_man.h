/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve Pettersen
 */
#ifndef _URL_DYNAMIC_ATTRIBUTE_MAN_H
#define _URL_DYNAMIC_ATTRIBUTE_MAN_H

class URL_DynamicUIntAttributeHandler;
class URL_DynamicStringAttributeHandler;
class URL_DynamicURLAttributeHandler;

// NTK: they descend from Link, since we
#include "modules/url/url_dynattr_int.h" // URL_DynamicURLAttributeDescriptor
#include "modules/url/url_dynattr_int.h" // URL_DynamicStringAttributeDescriptor
#include "modules/url/url_dynattr_int.h" // URL_DynamicUIntAttributeDescriptor

template <typename AttributeType, class Descriptor, class Handler>
class URL_SingleDynamicAttributeManager
{
private:
	/** List of attributes for integers */
	AutoDeleteHead attribute_index;

	/** What is the next enum value to be assigned to an attribute? */
	AttributeType next_attribute;

public:
	/** Constructor */
	URL_SingleDynamicAttributeManager(AttributeType start):next_attribute(start){}

	/** Destructor */
	virtual ~URL_SingleDynamicAttributeManager(){}

	/** Register a dynamic unsigned integer attribute handler for URLs.
	 *	Returns the assigned attribute enum
	 *	LEAVEs in case of errors
	 */
	AttributeType RegisterAttributeL(Handler *handler);

	/** Locate the identified attribute's descriptor, and return a pointer to it */
	const Descriptor *FindAttribute(AttributeType attr);

	/** Locate the identified attribute's descriptor, and return a pointer to it */
	BOOL FindDynAttribute(const Descriptor **, uint32 module_id, uint32 tag_id, BOOL create = FALSE);

	/** Get the first Attribute descriptor */
	void GetFirstAttributeDescriptor(Descriptor **desc){if(desc) *desc= static_cast<Descriptor *>(attribute_index.First());}

protected:

	/** Internal registration of a dynamic unsigned integer attribute handler for URLs. */
	OP_STATUS RegisterAttribute(AttributeType attr, Handler *handler);

private:
	/** Locate the identified attribute's descriptor, and return a pointer to it */
	Descriptor *InternalFindDynAttribute(uint32 module_id, uint32 tag_id);
};

typedef URL_SingleDynamicAttributeManager<URL::URL_Uint32Attribute, URL_DynamicUIntAttributeDescriptor, URL_DynamicUIntAttributeHandler> URL_DynamicIntAttributeManagerBase;
typedef URL_SingleDynamicAttributeManager<URL::URL_StringAttribute, URL_DynamicStringAttributeDescriptor, URL_DynamicStringAttributeHandler> URL_DynamicStringAttributeManager;
typedef URL_SingleDynamicAttributeManager<URL::URL_URLAttribute, URL_DynamicURLAttributeDescriptor, URL_DynamicURLAttributeHandler> URL_DynamicURLAttributeManager;

class  URL_DynamicIntAttributeManager : public URL_DynamicIntAttributeManagerBase
{
private:
	/** The attribute ID of the attribute used to store the bit flags */
	URL::URL_Uint32Attribute current_flag_attribute;

	/** What is the bit to be used for the next flag? Starts at the most signinifcant*/
	uint32 next_flag_mask;

	/** Default handler for the flag storage attribute (they are not cachable) */
	URL_DynamicUIntAttributeHandler *handler;

public:

	URL_DynamicIntAttributeManager(URL::URL_Uint32Attribute start_attr) :
				URL_DynamicIntAttributeManagerBase(start_attr),
				current_flag_attribute(URL::KNoInt),
				next_flag_mask(0),
				handler(NULL)
				{}

	virtual ~URL_DynamicIntAttributeManager();

	OP_STATUS GetNewFlagAttribute(URL::URL_Uint32Attribute &new_attr, uint32 &flag);
};

#define DYN_ATTRIBUTE_WRAPPERS(M, Attr, D, H) \
	Attr RegisterAttributeL(H *handler){return M::RegisterAttributeL(handler);}\
	const D *FindAttribute(Attr attr){return M::FindAttribute(attr);} \
	void GetFirstAttributeDescriptor(D **desc){M::GetFirstAttributeDescriptor(desc);}\
	BOOL FindDynAttribute(const D **desc, uint32 module_id, uint32 tag_id, BOOL create = FALSE){return M::FindDynAttribute(desc, module_id, tag_id, create);}

struct FlagHandlers;

/** Manager for the dynamic URL attributes. Maintains a list of the attributes */
class URL_DynamicAttributeManager :
	public URL_DynamicIntAttributeManager,
	public URL_DynamicStringAttributeManager,
	public URL_DynamicURLAttributeManager
{
public:
	/** Constructor */
	URL_DynamicAttributeManager();

	/** Destructor */
	virtual ~URL_DynamicAttributeManager();

	void InitL();

	DYN_ATTRIBUTE_WRAPPERS(URL_DynamicIntAttributeManager, URL::URL_Uint32Attribute, URL_DynamicUIntAttributeDescriptor, URL_DynamicUIntAttributeHandler)
	DYN_ATTRIBUTE_WRAPPERS(URL_DynamicStringAttributeManager, URL::URL_StringAttribute, URL_DynamicStringAttributeDescriptor, URL_DynamicStringAttributeHandler)
	DYN_ATTRIBUTE_WRAPPERS(URL_DynamicURLAttributeManager, URL::URL_URLAttribute, URL_DynamicURLAttributeDescriptor, URL_DynamicURLAttributeHandler)

private:
	OpAutoPtr<URL_DynamicUIntAttributeHandler> default_int_handler;
	OpAutoPtr<URL_DynamicStringAttributeHandler> default_str_handler;
	OpAutoPtr<URL_DynamicURLAttributeHandler> default_url_handler;
	OpAutoPtr<URL_DynamicStringAttributeHandler> httpmethod_str_handler;
	FlagHandlers *flags_list;
};

#endif // _URL_DYNAMIC_ATTRIBUTE_MAN_H
