/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef DOM_DOMSTRINGLIST
#define DOM_DOMSTRINGLIST

#include "modules/dom/src/domobj.h"

class DOM_DOMStringList
	: public DOM_Object
{
public:
	class Generator
	{
	public:
		virtual unsigned StringList_length() = 0;
		virtual OP_STATUS StringList_item(int index, const uni_char *&name) = 0;
		virtual BOOL StringList_contains(const uni_char *string) = 0;

	protected:
		virtual ~Generator() {}
		/* This destructor is only here because some compilers
		   complain if it isn't, not because it is in any way
		   needed. */
	};

	static OP_STATUS Make(DOM_DOMStringList *&stringlist, DOM_Object *base, Generator *generator, DOM_Runtime *runtime);
	/**< Create a new DOM_StringList.

	     @param [out] stringlist The result object.
	     @param base The object that must remain alive for the items
	            of the string list to be computed. Can be NULL if this
	            list doesn't have any such dependent objects.
	     @param generator The generator/producer object for this list,
	            @see DOM_StringList::Generator. If NULL, the string list will be empty.
	     @param runtime The runtime to create this list in.
	     @return OpStatus::OK if succesful, OpStatus::ERR_NO_MEMORY on OOM. */

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_GetState GetIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_DOMSTRINGLIST || DOM_Object::IsA(type); }
	virtual void GCTrace();

	virtual ES_GetState GetIndexedPropertiesLength(unsigned &count, ES_Runtime *origining_runtime);

	DOM_DECLARE_FUNCTION(item);
	DOM_DECLARE_FUNCTION(contains);
	enum { FUNCTIONS_ARRAY_SIZE = 3 };

protected:
	DOM_DOMStringList(DOM_Object *base, Generator *generator);

	DOM_Object *base;
	Generator *generator;
};

#endif // DOM_DOMSTRINGLIST
