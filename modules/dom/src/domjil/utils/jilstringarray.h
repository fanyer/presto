/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 * @author Tomasz Jamroszczak tjamroszczak@opera.com
 *
 */

#ifndef DOM_DOMJILSTRINGARRAY_H
#define DOM_DOMJILSTRINGARRAY_H

#ifdef DOM_JIL_API_SUPPORT

#include "modules/dom/src/domjil/domjilobject.h"
#include "modules/dom/src/domjil/utils/jilutils.h"

class DOM_JILString_Array : public DOM_Object
{
public:
	virtual BOOL IsA(int type) { return type == DOM_TYPE_JIL_STRING_ARRAY || DOM_Object::IsA(type); }
	static OP_STATUS Make(DOM_JILString_Array*& new_obj, DOM_Runtime* origining_runtime, const OpAutoVector<OpString>* from_array = NULL);
	static OP_STATUS Make(DOM_JILString_Array*& new_obj, DOM_Runtime* origining_runtime, ES_Object* from_object, int len);
	virtual ES_GetState GetName(OpAtom property_code, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_GetState GetIndex(int index, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutIndex(int property_index, ES_Value* value, ES_Runtime* origining_runtime);
	int GetLength() { return m_length; }
	BOOL GetIndex(UINT32 index, ES_Value*& result);
	OP_STATUS GetCopy(DOM_JILString_Array*& to_array, DOM_Runtime* origining_runtime);  // Allocates to_array.
	OP_STATUS GetCopy(OpAutoVector<OpString>* to_vector, DOM_Runtime* origining_runtime);  // Doesn't allocate to_vector.

	virtual ES_GetState FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime *origining_runtime);
	virtual ES_GetState GetIndexedPropertiesLength(unsigned &count, ES_Runtime *origining_runtime);

	virtual void GCTrace();
	void Empty();
	OP_STATUS Remove(const uni_char* to_remove);
	virtual ~DOM_JILString_Array();
private:
	int m_length;
	DOM_JILString_Array();
	int FindIndex(int to_find);

	OpAutoVector<DOM_JIL_Array_Element> m_array;
};

#endif // DOM_JIL_API_SUPPORT
#endif // DOM_DOMJILSTRINGARRAY_H
