/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XML_ERRORS

#include "modules/xmlutils/xmlerrorreport.h"
#include "modules/util/str.h"

XMLErrorReport::Item::~Item()
{
	OP_DELETEA(string);
}

XMLErrorReport::XMLErrorReport()
	: items_count(0),
	  items_allocated(0),
	  items(0)
{
}

XMLErrorReport::~XMLErrorReport()
{
	for (unsigned index = 0; index < items_count; ++index)
		OP_DELETE(items[index]);
	OP_DELETEA(items);
}

OP_STATUS
XMLErrorReport::AddItem(Item::Type type, unsigned line, unsigned column, const uni_char *string, unsigned string_length)
{
	if (string && string_length == ~0u)
		string_length = uni_strlen(string);

	if (items_count == items_allocated)
	{
		unsigned new_allocated = items_allocated == 0 ? 11 : items_allocated << 1;

		Item **new_items = OP_NEWA(Item *, new_allocated);
		if (!new_items)
			return OpStatus::ERR_NO_MEMORY;

		op_memcpy(new_items, items, items_count * sizeof items[0]);
		OP_DELETEA(items);

		items_allocated = new_allocated;
		items = new_items;
	}

	Item *item = OP_NEW(Item, ());
	if (!item)
		return OpStatus::ERR_NO_MEMORY;

	item->type = type;
	item->line = line;
	item->column = column;

	if (string && (type == Item::TYPE_LINE_FRAGMENT || type == Item::TYPE_LINE_END))
	{
		item->string = UniSetNewStrN(string, string_length);
		if (!item->string)
		{
			OP_DELETE(item);
			return OpStatus::ERR_NO_MEMORY;
		}
	}
	else
		item->string = 0;

	items[items_count++] = item;
	return OpStatus::OK;
}

#endif // XML_ERRORS
