/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Marius Blomli, Michal Zajaczkowski
 */

#include "core/pch.h"

#ifdef AUTO_UPDATE_SUPPORT

#include "adjunct/autoupdate/updatableresource.h"
#include "adjunct/autoupdate/updatablefile.h"
#include "adjunct/autoupdate/updatablesetting.h"
#include "modules/xmlutils/xmlfragment.h"
#include "modules/xmlutils/xmlnames.h"

/**
 * The string values representing the XML element names that may be found in the XML response.
 * The UpdatableResourceFactory::URAttrName[] array *MUST* be in sync with the URAttr enum!
 */
/* static */
const uni_char* UpdatableResourceFactory::URAttrName[] =
{
	UNI_L("type"),
	UNI_L("name"),
	UNI_L("folder"),
	UNI_L("url"),
	UNI_L("urlalt"),
	UNI_L("version"),
	UNI_L("bnum"),
	UNI_L("id"),
	UNI_L("size"),
	UNI_L("lang"),
	UNI_L("timestamp"),
	UNI_L("checksum"),
	UNI_L("infourl"),
	UNI_L("showdialog"),
	UNI_L("vendor-name"),
	UNI_L("install-params"),
	UNI_L("eula-url"),
	UNI_L("plugin-name"),
	UNI_L("mime-type"),
	UNI_L("installs-silently"),
	UNI_L("has-installer"),
	UNI_L("section"),
	UNI_L("key"),
	UNI_L("data"),
	UNI_L("file")
	/***CAREFUL*WHEN*ADDING*VALUES*HERE*SEE*THE*URAttr*ENUM***/
};

/* static */
OP_STATUS UpdatableResourceFactory::GetResourceFromXML(const OpStringC& resource_type, XMLFragment& fragment, UpdatableResource** resource)
{
	OpString name_string, value_string;
	OpAutoVector<URNameValuePair> pairs;

	while(fragment.EnterAnyElement())
	{
		XMLCompleteName element_name;
		element_name = fragment.GetElementName();

		RETURN_IF_ERROR(name_string.Set(element_name.GetLocalPart()));
		RETURN_IF_ERROR(value_string.Set(fragment.GetText()));

		URAttr attr = ConvertNameToAttr(name_string);
		if (URA_LAST == attr)
			return OpStatus::ERR;

		OpAutoPtr<URNameValuePair> new_pair(OP_NEW(URNameValuePair, ()));
		RETURN_OOM_IF_NULL(new_pair.get());

		new_pair->SetAttrName(attr);
		RETURN_IF_ERROR(new_pair->SetValue(value_string));

		RETURN_IF_ERROR(pairs.Add(new_pair.get()));
		new_pair.release();

		fragment.LeaveElement();
	}

	// Ugly, we get the type with files, but we don't get one with settings.
	OpString type_string;
	if (resource_type.CompareI(UNI_L("setting")) == 0)
		RETURN_IF_ERROR(type_string.Set(resource_type));
	else
	{
		OP_ASSERT(resource_type.CompareI(UNI_L("file")) == 0);
		RETURN_IF_ERROR(UpdatableResource::GetValueByAttr(pairs, URA_TYPE, type_string));
	}

	*resource = GetEmptyResourceByType(type_string);
	RETURN_OOM_IF_NULL(*resource);
	OpAutoPtr<UpdatableResource> resource_keeper(*resource);

	for (UINT32 i=0; i<pairs.GetCount(); i++)
	{
		URNameValuePair* pair = pairs.Get(i);
		OP_ASSERT(pair);
		RETURN_IF_ERROR((*resource)->AddNameValuePair(pair));
	}

	if (FALSE == (*resource)->VerifyAttributes())
		return OpStatus::ERR;

	resource_keeper.release();
	return OpStatus::OK;
}

/* static */
UpdatableResource* UpdatableResourceFactory::GetEmptyResourceByType(const OpStringC& type)
{
	UpdatableResource* new_resource = NULL;

	if (type.CompareI(UNI_L("full")) == 0)
		new_resource = OP_NEW(UpdatablePackage, ());
	else if (type.CompareI(UNI_L("spoof")) == 0)
		new_resource = OP_NEW(UpdatableSpoof, ());
	else if (type.CompareI(UNI_L("browserjs")) == 0)
		new_resource = OP_NEW(UpdatableBrowserJS, ());
	else if (type.CompareI(UNI_L("dictionary")) == 0)
		new_resource = OP_NEW(UpdatableDictionary, ());
	else if (type.CompareI(UNI_L("dictionaries")) == 0)
		new_resource = OP_NEW(UpdatableDictionaryXML, ());
#ifdef PLUGIN_AUTO_INSTALL
	else if (type.CompareI(UNI_L("plugin")) == 0)
		new_resource = OP_NEW(UpdatablePlugin, ());
#endif // PLUGIN_AUTO_INSTALL
	else if (type.CompareI(UNI_L("hardwareblocklist")) == 0)
		new_resource = OP_NEW(UpdatableHardwareBlocklist, ());
	else if (type.CompareI(UNI_L("setting")) == 0)
		new_resource = OP_NEW(UpdatableSetting, ());
	else if (type.CompareI(UNI_L("handlersignore")) == 0)
		new_resource = OP_NEW(UpdatableHandlersIgnore, ());
	else
		// Add a new type above if you are implementing a new resource type
		OP_ASSERT(!"Resource type string not recognized!");

	return new_resource;
}

/* static */
URAttr UpdatableResourceFactory::ConvertNameToAttr(const OpStringC& name)
{
	if (name.IsEmpty())
		return URA_LAST;

	int count = ARRAY_SIZE(URAttrName);

	// There a mess between the URAttr and UpdatableResourceFactory::URAttrName[] if this assert triggers!
	OP_ASSERT(URA_LAST == count);

	for (int i=0; i<count; i++)
	{
		if (name.CompareI(URAttrName[i]) == 0)
			return (URAttr)i;
	}

	// Add the correct elements to URAttr and UpdatableResourceFactory::URAttrName[] if you believe this should not trigger.
	OP_ASSERT(!"Unknown string attribute name received in the response XML!");
	return URA_LAST;
}

/********************
* UpdatableResource *
*********************/

UpdatableResource::UpdatableResource()
{
}

OP_STATUS UpdatableResource::AddNameValuePair(URNameValuePair* pair)
{
	OP_ASSERT(pair);
	
	if (m_pairs.GetCount() > UpdatableResourceFactory::URA_MAX_PROPERTY_COUNT)
	{
		OP_ASSERT(!"Property count limit reached!");
		return OpStatus::ERR;
	}

	OpAutoPtr<URNameValuePair> new_pair(OP_NEW(URNameValuePair, ()));
	RETURN_OOM_IF_NULL(new_pair.get());
	new_pair->SetAttrName(pair->GetAttrName());
	OpString value;
	RETURN_IF_ERROR(pair->GetAttrValue(value));
	RETURN_IF_ERROR(new_pair->SetValue(value));
	RETURN_IF_ERROR(m_pairs.Add(new_pair.get()));
	new_pair.release();
	return OpStatus::OK;
}

/* static */
OP_STATUS UpdatableResource::GetValueByAttr(OpAutoVector<URNameValuePair>& pairs, URAttr attr, OpString& value)
{
	for (UINT32 i=0; i<pairs.GetCount(); i++)
	{
		URNameValuePair* pair = pairs.Get(i);
		OP_ASSERT(pair);
		if (pair->IsAttr(attr))
			return pair->GetAttrValue(value);
	}
	
	return OpStatus::ERR_FILE_NOT_FOUND;
}

OP_STATUS UpdatableResource::GetAttrValue(URAttr attr, OpString& value)
{
	return GetValueByAttr(m_pairs, attr, value);
}

OP_STATUS UpdatableResource::GetAttrValue(URAttr attr, int& value)
{
	OpString value_str;
	RETURN_IF_ERROR(GetValueByAttr(m_pairs, attr, value_str));

	if (value_str.IsEmpty())
		return OpStatus::ERR;

	value = uni_atoi(value_str.CStr());
	return OpStatus::OK;
}

OP_STATUS UpdatableResource::GetAttrValue(URAttr attr, bool& value)
{
	OpString value_str;
	RETURN_IF_ERROR(GetValueByAttr(m_pairs, attr, value_str));

	if (value_str.IsEmpty())
		return OpStatus::ERR;

	if (value_str.CompareI("1") == 0 || value_str.CompareI("yes") == 0 || value_str.CompareI("true") == 0)
	{
		value = true;
		return OpStatus::OK;
	}

	if (value_str.CompareI("0") == 0 || value_str.CompareI("no") == 0 || value_str.CompareI("false") == 0)
	{
		value = false;
		return OpStatus::OK;
	}

	return OpStatus::ERR;

}

BOOL UpdatableResource::HasAttrWithContent(URAttr attr)
{
	OpString value;
	RETURN_VALUE_IF_ERROR(GetAttrValue(attr, value), FALSE);
	if (value.IsEmpty())
		return FALSE;

	return TRUE;
}

#endif // AUTO_UPDATE_SUPPORT
