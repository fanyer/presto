/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef DOM_DOMSTRINGMAP
#define DOM_DOMSTRINGMAP

#include "modules/dom/src/domobj.h"

class DOM_Element;

/**
 * In theory this is a generic class, but since it right now is only used for Element.dataset, it
 * really only implements that behaviour, merging the specs for DOMStringMap and Element.dataset.
 */
class DOM_DOMStringMap
	: public DOM_Object
{
private:
	DOM_Element *owner;

	DOM_DOMStringMap(DOM_Element *owner) : owner(owner) {}

public:
	static OP_STATUS Make(DOM_DOMStringMap *&stringmap, DOM_Element *owner);

	virtual ES_GetState GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_DeleteStatus DeleteName(const uni_char *property_name, ES_Runtime *origining_runtime);

	virtual ES_GetState FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime *origining_runtime);
	virtual void GCTrace();
};

#endif // DOM_DOMSTRINGMAP
