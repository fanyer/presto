/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.	All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/
#ifndef DOM_EXTENSIONS_TABAPICACHEDOBJECT_H
#define DOM_EXTENSIONS_TABAPICACHEDOBJECT_H

#ifdef DOM_EXTENSIONS_TAB_API_SUPPORT

#include "modules/dom/src/extensions/domextensionsupport.h"
#include "modules/windowcommander/OpExtensionUIListener.h"
#include "modules/dom/src/domobj.h"

class DOM_TabApiCachedObject
	: public DOM_Object
{
public:
	DOM_TabApiCachedObject(DOM_ExtensionSupport* extension_support, OpTabAPIListener::TabAPIItemId id)
		: m_extension_support(extension_support)
		, m_is_significant(FALSE)
		, m_id(id)
	{
		OP_ASSERT(extension_support);
		OP_ASSERT(id);
	}
	virtual ~DOM_TabApiCachedObject();

	BOOL IsSignificant() { return m_is_significant; }

	virtual ES_PutState PutName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
	{
		SetIsSignificant();
		return DOM_Object::PutName(property_name, property_code, value, origining_runtime);
	}

	// This is here only to make it visible in DOM_TabApiCachedObject.
	virtual ES_PutState PutName(OpAtom property_atom, ES_Value* value, ES_Runtime* origining_runtime)
	{
		return DOM_Object::PutName(property_atom, value, origining_runtime);
	}

	virtual ES_PutState PutIndex(int property_index, ES_Value* value, ES_Runtime* origining_runtime)
	{
		SetIsSignificant();
		return DOM_Object::PutIndex(property_index, value, origining_runtime);
	}

	OpTabAPIListener::TabAPIItemId GetId() { return m_id; }
protected:
	void SetIsSignificant() { m_is_significant = TRUE; }

	OpSmartPointerWithDelete<DOM_ExtensionSupport> m_extension_support;
private:
	BOOL m_is_significant;
	OpTabAPIListener::TabAPIItemId m_id;
};

#endif // DOM_EXTENSIONS_TAB_API_SUPPORT

#endif // DOM_EXTENSIONS_TABAPICACHEDOBJECT_H
