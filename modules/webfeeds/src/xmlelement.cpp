// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.

#include "core/pch.h"

#ifdef WEBFEEDS_BACKEND_SUPPORT

#include "modules/webfeeds/src/xmlelement.h"
#include "modules/webfeeds/src/webfeedparser.h"
#include "modules/webfeeds/src/webfeedutil.h"

#include "modules/logdoc/namespace.h"

//****************************************************************************
//
//	XMLElement
//
//****************************************************************************


XMLElement::XMLElement(WebFeedParser& owner)
:	m_owner(owner), m_attributes(TRUE)
{
}


XMLElement::~XMLElement()
{
}


OP_STATUS XMLElement::Init(const OpStringC& element_namespace,
	const OpStringC& local_name, const XMLToken::Attribute* attributes,
	UINT attributes_count)
{
	RETURN_IF_ERROR(m_namespace_uri.Set(element_namespace));
	RETURN_IF_ERROR(m_local_name.Set(local_name));
	RETURN_IF_ERROR(m_owner.QualifiedName(m_qualified_name, m_namespace_uri, m_local_name));

	// Loop the passed attributes and create new XMLAttributeQN objects
	// from them.
	for (UINT index = 0; index < attributes_count; ++index)
	{
		const XMLCompleteNameN& attrname = attributes[index].GetName();

		OpString namespace_uri;
		OpString attribute_name;
		OpString attribute_value;

		RETURN_IF_ERROR(namespace_uri.Set(attrname.GetUri(), attrname.GetUriLength()));
		RETURN_IF_ERROR(attribute_name.Set(attrname.GetLocalPart(), attrname.GetLocalPartLength()));
		RETURN_IF_ERROR(attribute_value.Set(attributes[index].GetValue(), attributes[index].GetValueLength()));

		if (attrname.GetNsIndex() == NS_IDX_XMLNS)
		{
			if (!m_owner.HasNamespacePrefix(attribute_value))
			{
				// The user have not set a specific prefix for this uri, so we use
				// the prefix the xml creator desired.
				RETURN_IF_ERROR(m_owner.AddNamespacePrefix(attribute_name, attribute_value));
			}
		}

		OpAutoPtr<XMLAttributeQN> attribute_qn(OP_NEW(XMLAttributeQN, ()));
		if (!attribute_qn.get())
			return OpStatus::ERR_NO_MEMORY;

		OpString qualified_name;
		RETURN_IF_ERROR(m_owner.QualifiedName(qualified_name, namespace_uri, attribute_name));
		RETURN_IF_ERROR(attribute_qn->Init(namespace_uri, attribute_name, qualified_name, attribute_value));

		OP_STATUS status = m_attributes.Add(attribute_qn->QualifiedName().CStr(), attribute_qn.get());
		RETURN_IF_MEMORY_ERROR(status);

		if (OpStatus::IsSuccess(status))
			attribute_qn.release();
	}

	// Check for xml:base attribute.
	if (m_attributes.Contains(UNI_L("xml:base")))
	{
		XMLAttributeQN* attribute = 0;
		RETURN_IF_ERROR(m_attributes.GetData(UNI_L("xml:base"), &attribute));

		OP_ASSERT(attribute != 0);
		const OpStringC& value = attribute->Value();

		// Convert the value into an absolute uri.
		OpString base_uri;
		RETURN_IF_ERROR(m_owner.BaseURI(base_uri));

		if (base_uri.HasContent())
			RETURN_IF_ERROR(WebFeedUtil::RelativeToAbsoluteURL(m_base_uri, value, base_uri));
		else
			RETURN_IF_ERROR(m_base_uri.Set(value));
	}
	
	return OpStatus::OK; 
}



//****************************************************************************
//
//	XMLAttributeQN
//
//****************************************************************************

XMLAttributeQN::XMLAttributeQN()
{
}


XMLAttributeQN::~XMLAttributeQN()
{
}


OP_STATUS XMLAttributeQN::Init(const OpStringC& namespace_uri,
	const OpStringC& local_name, const OpStringC& qualified_name,
	const OpStringC& value)
{
	RETURN_IF_ERROR(m_namespace_uri.Set(namespace_uri));
	RETURN_IF_ERROR(m_local_name.Set(local_name));
	RETURN_IF_ERROR(m_qualified_name.Set(qualified_name));
	RETURN_IF_ERROR(m_value.Set(value));

	return OpStatus::OK;
}

#endif  // WEBFEEDS_BACKEND_SUPPORT
