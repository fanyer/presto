/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "modules/dom/src/domobj.h"

/* static */ void
DOM_PropertyStorage::DeleteProperty(const void *key, void *data)
{
	Property *property = static_cast<Property *>(data);

	DOM_Object::DOMFreeValue(property->value);
	op_free(property);
}

DOM_PropertyStorage::~DOM_PropertyStorage()
{
	backend.ForEach(DeleteProperty);
}

/* static */ BOOL
DOM_PropertyStorage::Get(DOM_PropertyStorage *storage, const uni_char *name, ES_Value *value)
{
	if (storage)
	{
		void *data;

		if (storage->backend.GetData(name, &data) == OpStatus::OK)
		{
			if (value)
				*value = static_cast<Property *>(data)->value;

			return TRUE;
		}
	}

	return FALSE;
}

/* static */ OP_STATUS
DOM_PropertyStorage::Put(DOM_PropertyStorage *&storage, const uni_char *name, BOOL enumerable, const ES_Value &value)
{
	if (storage)
	{
		void *data;

		if (storage->backend.GetData(name, &data) == OpStatus::OK)
		{
			Property *property = static_cast<Property *>(data);
			ES_Value temporary = property->value;

			if (OpStatus::IsMemoryError(DOM_Object::DOMCopyValue(property->value, value)))
			{
				property->value = temporary;
				return OpStatus::ERR_NO_MEMORY;
			}

			property->enumerable = enumerable;
			DOM_Object::DOMFreeValue(temporary);
			return OpStatus::OK;
		}
	}

	if (!storage && !(storage = OP_NEW(DOM_PropertyStorage, ())))
		return OpStatus::ERR_NO_MEMORY;

	Property *property = static_cast<Property *>(op_malloc(sizeof(Property) + uni_strlen(name) * sizeof(uni_char)));
	if (!property || OpStatus::IsMemoryError(DOM_Object::DOMCopyValue(property->value, value)))
	{
		op_free(property);
		return OpStatus::ERR_NO_MEMORY;
	}
	property->enumerable = enumerable;
	uni_strcpy(property->name, name);

	if (OpStatus::IsMemoryError(storage->backend.Add(property->name, property)))
	{
		DOM_Object::DOMFreeValue(property->value);
		op_free(property);
		return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}

/* static */ void
DOM_PropertyStorage::FetchPropertiesL(ES_PropertyEnumerator *enumerator, DOM_PropertyStorage *storage)
{
	if (storage)
	{
		OpStackAutoPtr<OpHashIterator> iterator(storage->backend.GetIterator());
		if (iterator.get())
		{
			OP_STATUS status = iterator->First();
			while (OpStatus::IsSuccess(status))
			{
				Property *property = static_cast<Property *>(iterator->GetData());
				if (property->enumerable)
					enumerator->AddPropertyL(property->name);

				status = iterator->Next();
			}
		}
	}
}

/* static */ void
DOM_PropertyStorage::GCTrace(DOM_Runtime *runtime, DOM_PropertyStorage *storage)
{
	if (storage)
	{
		OpStackAutoPtr<OpHashIterator> iterator(storage->backend.GetIterator());
		if (iterator.get())
		{
			OP_STATUS status = iterator->First();
			while (OpStatus::IsSuccess(status))
			{
				DOM_Object::GCMark(runtime, static_cast<Property *>(iterator->GetData())->value);
				status = iterator->Next();
			}
		}
	}
}

