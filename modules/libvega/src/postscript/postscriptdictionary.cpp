/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 * @author Anders Karlsson (pugo)
 */

#include "core/pch.h"

#ifdef VEGA_POSTSCRIPT_PRINTING

#include "modules/libvega/src/postscript/postscriptdictionary.h"


OP_STATUS PostScriptDictionary::add(const OpStringC8& key, const OpStringC8& value)
{
	OpAutoPtr<DictionaryItem> item (OP_NEW(DictionaryItem, ()));
	if (!item.get())
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(item->key.Set(key));
	RETURN_IF_ERROR(item->value.Set(value));

	RETURN_IF_ERROR(dictionary.Add(item.get()));
	item.release();

	return OpStatus::OK;
}


OP_STATUS PostScriptDictionary::add(const OpStringC8& key, long value)
{
	OpAutoPtr<DictionaryItem> item (OP_NEW(DictionaryItem, ()));
	if (!item.get())
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(item->key.Set(key));
	RETURN_IF_ERROR(item->value.AppendFormat("%ld", value));

	RETURN_IF_ERROR(dictionary.Add(item.get()));
	item.release();

	return OpStatus::OK;
}


OpStringC8 PostScriptDictionary::getValue(const OpStringC8& key) const
{
	for (unsigned i = 0; i < dictionary.GetCount(); i++)
	{
		DictionaryItem* item = dictionary.Get(i);
		if (item->key == key)
			return item->value;
	}

	OpStringC8 empty_string;
	return empty_string;
}

unsigned int PostScriptDictionary::size() const
{
	return dictionary.GetCount();
}


OP_STATUS PostScriptDictionary::generate(OpString8& result)
{
	RETURN_IF_ERROR(result.AppendFormat("%d dict dup begin\n", dictionary.GetCount()));
	for (unsigned i = 0; i < dictionary.GetCount(); i++)
	{
		DictionaryItem* item = dictionary.Get(i);
		result.AppendFormat("  /%s %s def\n", item->key.CStr(), item->value.CStr());
	}
	return result.Append("end");
}

#endif // VEGA_POSTSCRIPT_PRINTING
