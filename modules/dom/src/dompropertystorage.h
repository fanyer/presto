/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef DOMPROPERTYSTORAGE_H
#define DOMPROPERTYSTORAGE_H

#include "modules/util/OpHashTable.h"

class DOM_PropertyStorage
{
private:
	OpGenericStringHashTable backend;

	DOM_PropertyStorage()
		: backend(TRUE)
	{
	}

	class Property
	{
	public:
		ES_Value value;
		BOOL enumerable;
		uni_char name[1]; // ARRAY OK jl 2008-05-08 jl
	};

	static void DeleteProperty(const void *key, void *data);

public:
	~DOM_PropertyStorage();

	static BOOL Get(DOM_PropertyStorage *storage, const uni_char *name, ES_Value *value);
	static OP_STATUS Put(DOM_PropertyStorage *&storage, const uni_char *name, BOOL enumerable, const ES_Value &value);
	static void FetchPropertiesL(ES_PropertyEnumerator *enumerator, DOM_PropertyStorage *storage);

	static void GCTrace(DOM_Runtime *runtime, DOM_PropertyStorage *storage);
};

#endif // DOMPROPERTYSTORAGE_H
