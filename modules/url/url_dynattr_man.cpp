/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve Pettersen
 */

#include "core/pch.h"

#include "modules/url/url2.h"
#include "modules/url/url_man.h"
#include "modules/url/url_rep.h"
#include "modules/url/url_ds.h"

#include "modules/url/url_dynattr.h"
#include "modules/url/url_dynattr_int.h"
#include "modules/url/url_dynattr_man.h"

#include "modules/url/special_attrs.h"

#include "modules/url/tools/arrays.h"

struct FlagHandlers
{
	URL::URL_Uint32Attribute attr;
	OpAutoPtr<URL_DynamicUIntAttributeHandler> handler;
	
	FlagHandlers(): attr(URL::KNoInt){};

	OP_STATUS Init(URL::URL_Uint32Attribute _attr);
};


template <typename AttributeType, class Descriptor, class Handler>
AttributeType URL_SingleDynamicAttributeManager<AttributeType, Descriptor, Handler>::RegisterAttributeL(Handler *handler)
{
	if(handler == NULL)
		LEAVE(OpStatus::ERR_NULL_POINTER);

	OP_ASSERT(handler->GetModuleId() <= USHRT_MAX); // If we ever get to 65K modules we have a !serious! design problem
	OP_ASSERT(handler->GetTagID() <= USHRT_MAX); // If we ever get to 65K tags in a module we have another serious problem: feature creep.

	if(handler->GetModuleId() > USHRT_MAX || handler->GetTagID() > USHRT_MAX)
		LEAVE(OpStatus::ERR_OUT_OF_RANGE);

	if(handler->GetModuleId() != 0)
	{
		Descriptor *desc = InternalFindDynAttribute(handler->GetModuleId(), handler->GetTagID());

		if(desc)
		{
			LEAVE_IF_ERROR(desc->Construct(handler));
			return desc->GetAttributeID();
		}
	}

	AttributeType attr_num = next_attribute;
	next_attribute =  (AttributeType) (((unsigned int) next_attribute) +1);

	LEAVE_IF_ERROR(RegisterAttribute(attr_num, handler));

	return attr_num;
}

template <typename AttributeType, class Descriptor, class Handler>
const Descriptor *URL_SingleDynamicAttributeManager<AttributeType, Descriptor, Handler>::FindAttribute(AttributeType attr)
{
	Descriptor *item = NULL;
	
	GetFirstAttributeDescriptor(&item);

	while(item)
	{
		if(item->GetAttributeID() == attr)
			return item;
		item = item->Suc();
	}
	return NULL;
}

template <typename AttributeType, class Descriptor, class Handler>
BOOL URL_SingleDynamicAttributeManager<AttributeType, Descriptor, Handler>::FindDynAttribute(const Descriptor **ret_desc, uint32 module_id, uint32 tag_id, BOOL create)
{
	if(ret_desc)
		*ret_desc = NULL;

	Descriptor *desc = InternalFindDynAttribute(module_id, tag_id);

	if(desc == NULL && create)
	{
		AttributeType attr_num= next_attribute;
		next_attribute= (AttributeType) (((unsigned int) next_attribute) +1);

		desc = OP_NEW(Descriptor, (attr_num, module_id, tag_id, TRUE));
		if(desc)
		{
			if(OpStatus::IsError(desc->Construct(NULL)))
			{
				OP_DELETE(desc);
				return FALSE;
			}
			desc->Into(&attribute_index);
		}
	}
	if(ret_desc)
		*ret_desc = desc;

	return (desc != NULL ? TRUE : FALSE);
}

template <typename AttributeType, class Descriptor, class Handler>
Descriptor *URL_SingleDynamicAttributeManager<AttributeType, Descriptor, Handler>::InternalFindDynAttribute(uint32 module_id, uint32 tag_id)
{
	Descriptor *item = NULL;
	
	GetFirstAttributeDescriptor(&item);

	while(item)
	{
		if(item->GetModuleId() == module_id && item->GetTagID() == tag_id)
			return item;
		item = item->Suc();
	}

	return NULL;
}

template <typename AttributeType, class Descriptor, class Handler>
OP_STATUS URL_SingleDynamicAttributeManager<AttributeType, Descriptor, Handler>::RegisterAttribute(AttributeType attr, Handler *handler)
{
	if(handler == NULL)
		return OpStatus::ERR_NULL_POINTER;

	if(handler->GetModuleId() != 0 && handler->GetTagID() != 0)
	{
		Descriptor *item = NULL;
		GetFirstAttributeDescriptor(&item);

		while(item)
		{
			if(item->GetAttributeID() == attr ||
				(item->GetModuleId() == handler->GetModuleId() &&
				item->GetTagID() == handler->GetTagID()))
			{
				OP_ASSERT(!"Duplicate Attribute values or duplicate module+tag IDs are not allowed");
				return OpStatus::ERR;
			}
			item = item->Suc();
		}
	}
	else
	{
		if(handler->GetCachable())
		{
			OP_ASSERT(!"Zero module or tagged attributes cannot be cached");
			return OpStatus::ERR;
		}
	}
	
	OpAutoPtr<Descriptor> new_item(OP_NEW(Descriptor, (attr, handler->GetModuleId(), handler->GetTagID(), FALSE)));

	if(!new_item.get())
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(new_item->Construct(handler));

	new_item.release()->Into(&attribute_index);
	return OpStatus::OK;
}

URL_DynamicIntAttributeManager::~URL_DynamicIntAttributeManager()
{
	OP_DELETE(handler);
	handler = NULL;
	current_flag_attribute = URL::KNoInt;
}

OP_STATUS URL_DynamicIntAttributeManager::GetNewFlagAttribute(URL::URL_Uint32Attribute &new_attr, uint32 &flag)
{
	if(handler == NULL)
	{
		handler = OP_NEW(URL_DynamicUIntAttributeHandler, (0, 0, FALSE));
		if(handler == NULL)
			return OpStatus::ERR_NO_MEMORY;
	}

	if(current_flag_attribute == URL::KNoInt)
	{
		TRAP_AND_RETURN(op_err, current_flag_attribute = RegisterAttributeL(handler));

		next_flag_mask = (UINT_MAX ^(UINT_MAX >> 1)); // Produces MSB
	}

	new_attr = current_flag_attribute;
	flag = next_flag_mask;

	next_flag_mask >>= 1;

	if(next_flag_mask == 0)
		current_flag_attribute = URL::KNoInt;

	return OpStatus::OK;
}

URL_DynamicAttributeManager::URL_DynamicAttributeManager():
	URL_DynamicIntAttributeManager(URL::KFirstUintDynamicAttributeItem),
	URL_DynamicStringAttributeManager(URL::KFirstStrDynamicAttributeItem),
	URL_DynamicURLAttributeManager(URL::KFirstURLDynamicAttributeItem),
	flags_list(NULL)
{
}

URL_DynamicAttributeManager::~URL_DynamicAttributeManager()
{
	OP_DELETEA(flags_list);
	flags_list = NULL;
}

void URL_DynamicAttributeManager::InitL()
{
	uint32 attr;
	
	default_int_handler.reset(OP_NEW_L(URL_DynamicUIntAttributeHandler, ()));
	default_str_handler.reset(OP_NEW_L(URL_DynamicStringAttributeHandler, ()));
	default_url_handler.reset(OP_NEW_L(URL_DynamicURLAttributeHandler, ()));

	for(attr = URL::KStartUintFullDynamicIntegers+1; attr < URL::KEndUintFullDynamicIntegers; attr++)
	{
		LEAVE_IF_ERROR(URL_DynamicIntAttributeManager::RegisterAttribute((URL::URL_Uint32Attribute) attr, default_int_handler.get()));
	}

	if(URL::KStartUintDynamicFlags+1 < URL::KEndUintDynamicFlags)
	{
		int i;
		flags_list = OP_NEWA_L(	FlagHandlers, (URL::KEndUintDynamicFlags- URL::KStartUintDynamicFlags -1));

		for(i=0, attr = URL::KStartUintDynamicFlags+1; attr < URL::KEndUintDynamicFlags; attr++, i++)
		{
			LEAVE_IF_ERROR(flags_list[i].Init((URL::URL_Uint32Attribute) attr));
			LEAVE_IF_ERROR(URL_DynamicIntAttributeManager::RegisterAttribute((URL::URL_Uint32Attribute) attr, flags_list[i].handler.get()));
		}
	}
	for(attr = URL::KStartStrDynamicString+1; attr < URL::KEndStrDynamicString; attr++)
	{
		LEAVE_IF_ERROR(URL_DynamicStringAttributeManager::RegisterAttribute((URL::URL_StringAttribute) attr, default_str_handler.get()));
	}
	for(attr = URL::KStartURLDynamicURLs+1; attr < URL::KEndURLDynamicURLs; attr++)
	{
		LEAVE_IF_ERROR(URL_DynamicURLAttributeManager::RegisterAttribute((URL::URL_URLAttribute) attr, default_url_handler.get()));
	}

	// HTTP method string attribute handler
	httpmethod_str_handler.reset(OP_NEW_L(HTTP_MethodStrAttrHandler, ()));
	LEAVE_IF_ERROR(URL_DynamicStringAttributeManager::RegisterAttribute(URL::KHTTPSpecialMethodStr, httpmethod_str_handler.get()));
}

OP_STATUS FlagHandlers::Init(URL::URL_Uint32Attribute _attr)
{
	attr = _attr;
	handler.reset(OP_NEW(URL_DynamicUIntAttributeHandler,()));
	if(handler.get() == NULL)
		return OpStatus::ERR_NO_MEMORY;
	handler->SetIsFlag(TRUE);
	return OpStatus::OK;
}

#ifdef ADS12
//ADS compiler doesn't handle templates well, this function is needed to initialize templates used.
void AdsCompilerWorkaround_url_dynattr_man()
{
	URL_SingleDynamicAttributeManager<URL::URL_Uint32Attribute, URL_DynamicUIntAttributeDescriptor, URL_DynamicUIntAttributeHandler> var1((URL::URL_Uint32Attribute)1);
	var1.RegisterAttributeL((URL_DynamicUIntAttributeHandler*)0);
	UINT32 mod_id = 0;
	UINT32 tag_id = 0;
	const URL_DynamicUIntAttributeDescriptor *desc1 = NULL;
	var1.FindDynAttribute(&desc1, mod_id, tag_id, TRUE);
	var1.FindAttribute((URL::URL_Uint32Attribute)1);

	URL_SingleDynamicAttributeManager<URL::URL_StringAttribute, URL_DynamicStringAttributeDescriptor, URL_DynamicStringAttributeHandler> var2((URL::URL_StringAttribute)1);
	var2.RegisterAttributeL((URL_DynamicStringAttributeHandler*)0);
	const URL_DynamicStringAttributeDescriptor *desc2 = NULL;
	var2.FindDynAttribute(&desc2, mod_id, tag_id, TRUE);
	var2.FindAttribute((URL::URL_StringAttribute)1);

	URL_SingleDynamicAttributeManager<URL::URL_URLAttribute, URL_DynamicURLAttributeDescriptor, URL_DynamicURLAttributeHandler> var3((URL::URL_URLAttribute)1);
	var3.RegisterAttributeL((URL_DynamicURLAttributeHandler*)0);
	const URL_DynamicURLAttributeDescriptor *desc3 = NULL;
	var3.FindDynAttribute(&desc3, mod_id, tag_id, TRUE);
	var3.FindAttribute((URL::URL_URLAttribute)1);
}
#endif

// Template instantiations
template class URL_SingleDynamicAttributeManager<URL::URL_Uint32Attribute, URL_DynamicUIntAttributeDescriptor, URL_DynamicUIntAttributeHandler>;
template class URL_SingleDynamicAttributeManager<URL::URL_StringAttribute, URL_DynamicStringAttributeDescriptor, URL_DynamicStringAttributeHandler>;
template class URL_SingleDynamicAttributeManager<URL::URL_URLAttribute, URL_DynamicURLAttributeDescriptor, URL_DynamicURLAttributeHandler>;

/* The functions below has been moved here because their use of template function defined above means that they cannot 
 * be linked correctly in some compilers if the instantiations are in a different compilation unit.
 */

URL::URL_Uint32Attribute URL_API::RegisterAttributeL(URL_DynamicUIntAttributeHandler *handler)
{
	return urlManager->RegisterAttributeL(handler);
}


#if defined URL_ENABLE_DYNATTR_SEND_HEADER || defined URL_ENABLE_DYNATTR_SEND_MULTIHEADER
#define CONST_KEYWORD_ARRAY(name) PREFIX_CONST_ARRAY(static, KeywordIndex, name, url)
#define CONST_KEYWORD_ENTRY(x,y) CONST_DOUBLE_ENTRY(keyword, x, index, y)
#define CONST_KEYWORD_END(name) CONST_END(name)

CONST_KEYWORD_ARRAY(Dynattr_Blocked_HTTPHeader)
	CONST_KEYWORD_ENTRY(NULL, FALSE)
	//CONST_KEYWORD_ENTRY("Authorization",TRUE)
	CONST_KEYWORD_ENTRY("Connection",TRUE)
	CONST_KEYWORD_ENTRY("Content-Length",TRUE)
	CONST_KEYWORD_ENTRY("Cookie",TRUE)
	CONST_KEYWORD_ENTRY("Cookie2",TRUE)
	CONST_KEYWORD_ENTRY("Date",TRUE)
	CONST_KEYWORD_ENTRY("Expect",TRUE)
	CONST_KEYWORD_ENTRY("Host",TRUE)
	//CONST_KEYWORD_ENTRY("If-Modified-Since",TRUE)
	//CONST_KEYWORD_ENTRY("If-None-Match",TRUE)
	//CONST_KEYWORD_ENTRY("If-Range",TRUE)
	CONST_KEYWORD_ENTRY("Keep-Alive",TRUE)
	//CONST_KEYWORD_ENTRY("Proxy-Authorization",TRUE)
	CONST_KEYWORD_ENTRY("Proxy-Connection",TRUE)
	CONST_KEYWORD_ENTRY("Range",TRUE)
	CONST_KEYWORD_ENTRY("Referer",TRUE)
	CONST_KEYWORD_ENTRY("TE",TRUE)
	CONST_KEYWORD_ENTRY("Trailer",TRUE)
	CONST_KEYWORD_ENTRY("Transfer-Encoding",TRUE)
	CONST_KEYWORD_ENTRY("Upgrade",TRUE)
	CONST_KEYWORD_ENTRY("User-Agent",TRUE)
	CONST_KEYWORD_ENTRY("Via",TRUE)
CONST_KEYWORD_END(Dynattr_Blocked_HTTPHeader)

BOOL DynAttrCheckBlockedHeader(const OpStringC8 &hdr_name)
{
	return hdr_name.HasContent() && 
			CheckKeywordsIndex(hdr_name.CStr(), 
				g_Dynattr_Blocked_HTTPHeader, 
				CONST_ARRAY_SIZE(url,Dynattr_Blocked_HTTPHeader));
}
#endif

URL::URL_StringAttribute URL_API::RegisterAttributeL(URL_DynamicStringAttributeHandler *handler)
{
#if defined URL_ENABLE_DYNATTR_SEND_HEADER || defined URL_ENABLE_DYNATTR_SEND_MULTIHEADER
	if(handler && handler->GetSendHTTP())
	{
		OpStringC8 hdr_name = handler->GetHTTPHeaderName();

		if(DynAttrCheckBlockedHeader(hdr_name))
			LEAVE(OpStatus::ERR_OUT_OF_RANGE);
	}
#endif
	return urlManager->RegisterAttributeL(handler);
}

URL::URL_URLAttribute URL_API::RegisterAttributeL(URL_DynamicURLAttributeHandler *handler)
{
	return urlManager->RegisterAttributeL(handler);
}

template <class DescType, class PayloadType> 
URL_DataStorage::URL_DynAttributeElement<DescType, PayloadType>::~URL_DynAttributeElement()
{
	if(InList())
		Out();
}

template <class DescType, class PayloadType> 
typename URL_DataStorage::URL_DynAttributeElement<DescType, PayloadType>::element *
URL_DataStorage::URL_DynAttributeElement<DescType, PayloadType>::FindElement(const DescType *desc)
{
	size_t i;
	for(i=0;i< ARRAY_SIZE(attributes); i++)
		if(attributes[i].desc == desc)
			return &attributes[i];
	return NULL;
}

template <class DescType, class PayloadType> 
BOOL URL_DataStorage::URL_DynAttributeElement<DescType, PayloadType>::InsertElement(const DescType *desc, PayloadType &val)
{
	if(desc == NULL)
		return TRUE;

	size_t  item_index;
	element *elm = NULL;
	for(item_index=0;item_index< ARRAY_SIZE(attributes); item_index++)
	{
		if(attributes[item_index].desc == NULL)
		{
			elm = &attributes[item_index];
			break;
		}
	}

	if(elm == NULL)
		return FALSE;

	elm->desc = desc;
	elm->value.TakeOver(val);

	return TRUE;
}

template <class DescType, class PayloadType> 
BOOL URL_DataStorage::URL_DynAttributeElement<DescType, PayloadType>::DeleteElement(const DescType *desc, URL_DataStorage::URL_DynAttributeElement<DescType, PayloadType> *last_element)
{
	if(last_element == this)
		last_element = NULL;

	OP_ASSERT(!last_element || last_element->attributes[0].desc != NULL);
	OP_ASSERT(!last_element || last_element->Suc() == NULL);
	OP_ASSERT(last_element || Suc() == NULL);

	size_t  item_index;
	element *elm = NULL;
	for(item_index=0;item_index< ARRAY_SIZE(attributes); item_index++)
		if(attributes[item_index].desc == desc)
		{
			elm = &attributes[item_index];
			break;
		}

	OP_ASSERT(elm);

	if(elm) do
	{
		if(last_element)
		{
			element *elm2 = NULL;
			size_t  i;
			for(i=0;i< ARRAY_SIZE(attributes)-1; i++)
			{
				if(last_element->attributes[i+1].desc == NULL)
				{
					elm2 = &last_element->attributes[i];
					break;
				}
			}

			OP_ASSERT(elm2);

			elm->TakeOver(elm2);

			if(last_element->attributes[0].desc == NULL)
				OP_DELETE(last_element);
		}
		else if(item_index< ARRAY_SIZE(attributes)-1)
		{
			for(;item_index< ARRAY_SIZE(attributes)-1;item_index++)
			{
				attributes[item_index].TakeOver(&attributes[item_index+1]);
			}
		}
		else
			elm->TakeOver(NULL);
	} while(0);

	return attributes[0].desc != NULL;
}


template struct URL_DataStorage::URL_DynAttributeElement<URL_DynamicUIntAttributeDescriptor, URL_DataStorage::UIntElement>;
template struct URL_DataStorage::URL_DynAttributeElement<URL_DynamicStringAttributeDescriptor, URL_DataStorage::StringElement>;
template struct URL_DataStorage::URL_DynAttributeElement<URL_DynamicURLAttributeDescriptor, URL_DataStorage::URL_Element>;

void URL_DataStorage::AddDynamicAttributeIntL(UINT32 mod_id, UINT32 tag_id, UINT32 value)
{
	UIntElement val(value);

	const URL_DynamicUIntAttributeDescriptor *desc = NULL;
	urlManager->FindDynAttribute(&desc, mod_id, tag_id, TRUE);
	if(desc)
		LEAVE_IF_ERROR(dynamic_int_attrs.UpdateValue(desc, val));
}

void URL_DataStorage::AddDynamicAttributeStringL(UINT32 mod_id, UINT32 tag_id, const OpStringC8& str)
{
	StringElement val;
	ANCHOR(StringElement, val);

	LEAVE_IF_ERROR(val.value.Set(str));

	const URL_DynamicStringAttributeDescriptor *desc = NULL;
	urlManager->FindDynAttribute(&desc, mod_id, tag_id, TRUE);
	if(desc)
		LEAVE_IF_ERROR(dynamic_string_attrs.UpdateValue(desc, val));
}

void URL_DataStorage::AddDynamicAttributeURL_L(UINT32 mod_id, UINT32 tag_id, const OpStringC8& str)
{
	URL_Element val;
	ANCHOR(URL_Element, val);

	LEAVE_IF_ERROR(val.value_str.Set(str));

	const URL_DynamicURLAttributeDescriptor *desc = NULL;
	urlManager->FindDynAttribute(&desc, mod_id, tag_id, TRUE);
	if(desc)
		LEAVE_IF_ERROR(dynamic_url_attrs.UpdateValue(desc, val));
}

void URL_DataStorage::AddDynamicAttributeFlagL(UINT32 mod_id, UINT32 tag_id)
{
	const URL_DynamicUIntAttributeDescriptor *desc = NULL;
	
	urlManager->FindDynAttribute(&desc, mod_id, tag_id, TRUE);
	
	if(desc)
	{
		if(!desc->GetIsFlag())
			LEAVE_IF_ERROR(((URL_DynamicUIntAttributeDescriptor *) desc)->SetIsFlag(TRUE)); // Need to override this attribute

		SetAttributeL(desc->GetAttributeID(), TRUE);
	}
}

uint32 URL_DataStorage::GetDynAttribute(URL::URL_Uint32Attribute attr) const
{
	OP_ASSERT(attr>URL::KStartUintDynamicAttributeList);
	const URL_DynamicUIntAttributeDescriptor *desc = urlManager->FindAttribute(attr);
	if(!desc)
		return 0;

	URL_DynIntAttributeElement::element *item = dynamic_int_attrs.FindElement(desc, NULL);
	uint32 val = (item ? ((uint32) item->value) : 0);

	URL this_url(url, (char *) NULL);
	RETURN_VALUE_IF_ERROR(desc->OnGetValue(this_url, val), 0);
	return val;
}

OP_STATUS URL_DataStorage::SetDynAttribute(URL::URL_Uint32Attribute attr, uint32 value)
{
	OP_ASSERT(attr>URL::KStartUintDynamicAttributeList);
	const URL_DynamicUIntAttributeDescriptor *desc = urlManager->FindAttribute(attr);
	if(!desc)
		return OpStatus::OK;

	URL this_url(url, (char *) NULL);
	uint32 val = value;
	BOOL set = FALSE;

	RETURN_IF_ERROR(desc->OnSetValue(this_url, this, val, set));

	if(set)
	{
		if(val) // Set non-zero
		{
			UIntElement val_item(val);

			RETURN_IF_ERROR(dynamic_int_attrs.UpdateValue(desc, val_item));
		}
		else
			dynamic_int_attrs.RemoveValue(desc);// Delete zero items; saves space.
	}
	return OpStatus::OK;
}

OP_STATUS URL_DataStorage::GetDynAttribute(URL::URL_StringAttribute attr, OpString8 &val) const
{
	OP_ASSERT(attr>URL::KStartStrDynamicAttributeList);
	val.Empty();

	const URL_DynamicStringAttributeDescriptor *desc = urlManager->FindAttribute(attr);
	if(!desc)
		return OpStatus::OK;

	URL_DynStrAttributeElement::element *item = dynamic_string_attrs.FindElement(desc, NULL);

	OpString8 ret_val;
	if(item)
		RETURN_IF_ERROR(ret_val.Set(item->value));

	URL this_url(url, (char *) NULL);
	RETURN_IF_ERROR(desc->OnGetValue(this_url, ret_val));
	val.TakeOver(ret_val);
	return OpStatus::OK;
}

OP_STATUS URL_DataStorage::SetDynAttribute(URL::URL_StringAttribute attr, const OpStringC8 &string)
{
	OP_ASSERT(attr>URL::KStartStrDynamicAttributeList);
	const URL_DynamicStringAttributeDescriptor *desc = urlManager->FindAttribute(attr);
	if(!desc)
		return OpStatus::OK;

	URL this_url(url, (char *) NULL);
	OpString8 val;
	BOOL set = FALSE;

	RETURN_IF_ERROR(val.Set(string));

	RETURN_IF_ERROR(desc->OnSetValue(this_url, val, set));

	if(set)
	{
		if(val.HasContent()) // Set non-zero
		{
			StringElement val_item;
			val_item.TakeOver(val);

			RETURN_IF_ERROR(dynamic_string_attrs.UpdateValue(desc, val_item));
		}
		else
			dynamic_string_attrs.RemoveValue(desc);// Delete zero items; saves space.
	}
	return OpStatus::OK;
}

URL URL_DataStorage::GetDynAttribute(URL::URL_URLAttribute  attr)
{
	OP_ASSERT(attr>URL::KStartURLDynamicAttributeList);
	const URL_DynamicURLAttributeDescriptor *desc = urlManager->FindAttribute(attr);
	if(!desc)
		return URL();

	URL_DynURLAttributeElement::element *item = dynamic_url_attrs.FindElement(desc, NULL);

	URL val;
	if(item)
	{
		if(item->value.value.IsValid())
			val = item->value.value;
		else
		{
			if(item->value.value_str.HasContent())
			{
				val = g_url_api->GetURL(item->value.value_str, url->GetContextId());
				if(val.IsValid())
				{
					item->value.value = val;
					item->value.value_str.Empty();
				}
			}
		}
	}
	URL this_url(url, (char *) NULL);
	RETURN_VALUE_IF_ERROR(desc->OnGetValue(this_url, val), URL());
	return val;
}

OP_STATUS URL_DataStorage::SetDynAttribute(URL::URL_URLAttribute  attr, const URL &param)
{
	OP_ASSERT(attr>URL::KStartURLDynamicAttributeList);
	const URL_DynamicURLAttributeDescriptor *desc = urlManager->FindAttribute(attr);
	if(!desc)
		return OpStatus::OK;

	URL this_url(url, (char *) NULL);
	URL val = param;
	BOOL set = FALSE;

	RETURN_IF_ERROR(desc->OnSetValue(this_url, val, set));

	if(set)
	{
		if(val.IsValid()) // Set non-zero
		{
			URL_Element val_item;
			val_item.TakeOver(val);

			RETURN_IF_ERROR(dynamic_url_attrs.UpdateValue(desc, val_item));
		}
		else
			dynamic_url_attrs.RemoveValue(desc);// Delete zero items; saves space.
	}
	return OpStatus::OK;
}
