/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef NS4P_COMPONENT_PLUGINS

#include "modules/ns4plugins/component/identifier_cache.h"
#include "modules/ns4plugins/component/browser_functions.h"

NPIdentifier
IdentifierCache::Bind(const PluginIdentifier& identifier, const NPUTF8* string /* = NULL */)
{
	Identifier* id = NULL;

	/* First attempt to look up an existing entry by browser identifier. */
	IdentifierMap::Iterator existing_id = m_identifier_map.Find(identifier.GetIdentifier());
	if (existing_id != m_identifier_map.End())
		id = existing_id->second;

	/* Then look up by value to check if we can match the new identifier to an orphan. */
	if (!id)
	{
		NPIdentifier maybe_orphan = NULL;
		if (!identifier.GetIsString())
			maybe_orphan = Lookup(identifier.GetIntValue());
		else if (string)
			maybe_orphan = Lookup(string);

		if (maybe_orphan && IsOrphan(maybe_orphan))
		{
			id = static_cast<Identifier*>(maybe_orphan);
			id->browser_identifier = identifier.GetIdentifier();

			OpStatus::Ignore(m_identifier_map.Insert(id->browser_identifier, id));
		}
	}

	/* If nothing was found, create new identifier. */
	if (!id)
	{
		id = OP_NEW(Identifier, ());
		RETURN_VALUE_IF_NULL(id, NULL);

		op_memset(id, 0, sizeof(*id));
		id->browser_identifier = identifier.GetIdentifier();
		id->is_string = !!identifier.GetIsString();

		/* No error checking since cache failure is acceptable. */
		OpStatus::Ignore(m_identifier_map.Insert(identifier.GetIdentifier(), id));
		OpStatus::Ignore(m_value_identifier_map.Insert(*id, id));
	}

	OP_ASSERT(id->is_string == !!identifier.GetIsString());

	/* Update contents, if necessary. */
	if (id->is_string && string)
		SetString(id, string);
	else if (!id->is_string && id->value.integer != identifier.GetIntValue())
		SetInteger(id, identifier.GetIntValue());

	return id;
}

void
IdentifierCache::SetInteger(NPIdentifier identifier, int32_t integer)
{
	if (Identifier* id = static_cast<Identifier*>(identifier))
	{
		if (!id->is_string && id->value.integer != integer)
		{
			m_value_identifier_map.Erase(*id);

			id->value.integer = integer;

			/* Re-index. Ignore return value since failure to cache is not fatal. */
			OpStatus::Ignore(m_value_identifier_map.Insert(*id, id));
		}
	}
}

void
IdentifierCache::SetString(NPIdentifier identifier, const NPUTF8* string)
{
	if (Identifier* id = static_cast<Identifier*>(identifier))
	{
		if (id->is_string)
		{
			m_value_identifier_map.Erase(*id);

			op_free(id->value.string);

			/* Return value of op_strdup ignored because failure to cache is not fatal. */
			id->value.string = string ? op_strdup(string) : NULL;

			/* Re-index. Ignore return value since failure to cache is not fatal. */
			OpStatus::Ignore(m_value_identifier_map.Insert(*id, id));
		}
	}
}

NPIdentifier
IdentifierCache::Lookup(int value)
{
	Identifier key;
	key.is_string = false;
	key.value.integer = value;

	ValueIdentifierMap::Iterator id = m_value_identifier_map.Find(key);
	return id != m_value_identifier_map.End() ? id->second : NULL;
}

NPIdentifier
IdentifierCache::Lookup(const NPUTF8* value)
{
	Identifier key;
	key.is_string = true;
	key.value.string = const_cast<NPUTF8*>(value);

	ValueIdentifierMap::Iterator id = m_value_identifier_map.Find(key);
	return id != m_value_identifier_map.End() ? id->second : NULL;
}

/* static */ UINT64
IdentifierCache::GetBrowserIdentifier(NPIdentifier identifier)
{
	return static_cast<Identifier*>(identifier)->browser_identifier;
}

/* static */ bool
IdentifierCache::IsString(NPIdentifier identifier)
{
	return static_cast<Identifier*>(identifier)->is_string;
}

/* static */ int32_t
IdentifierCache::GetInteger(NPIdentifier identifier)
{
	return static_cast<Identifier*>(identifier)->value.integer;
}

/* static */ NPUTF8*
IdentifierCache::GetString(NPIdentifier identifier)
{
	return static_cast<Identifier*>(identifier)->value.string;
}

/* static */ NPUTF8*
IdentifierCache::GetStringCopy(NPIdentifier identifier)
{
	size_t len = op_strlen(GetString(identifier));
	NPUTF8* ret = static_cast<NPUTF8*>(BrowserFunctions::NPN_MemAlloc(len + 1));
	RETURN_VALUE_IF_NULL(ret, NULL);
	op_memcpy(ret, GetString(identifier), len);
	ret[len] = '\0';

	return ret;
}

void
IdentifierCache::Unbind(UINT64 identifier)
{
	IdentifierMap::Iterator existing_id = m_identifier_map.Find(identifier);
	if (existing_id != m_identifier_map.End())
	{
		Identifier* id = existing_id->second;

		m_identifier_map.Erase(identifier);
		m_value_identifier_map.Erase(*id);

		if (id->is_string)
			op_free(id->value.string);

		OP_DELETE(id);
	}
}

bool
IdentifierCache::Identifier::operator<(const IdentifierCache::Identifier& o) const
{
	if (is_string)
	{
		if (!o.is_string)
			return false;

		if (!value.string || !o.value.string)
			return value.string < o.value.string;

		return op_strcmp(value.string, o.value.string) < 0;
	}
	else
	{
		if (o.is_string)
			return true;

		return value.integer < o.value.integer;
	}
}

void
IdentifierCache::Clear()
{
	m_value_identifier_map.Clear();

	for (IdentifierMap::Iterator i = m_identifier_map.Begin(); i != m_identifier_map.End(); i++)
		OP_DELETE(i->second);
	m_identifier_map.Clear();
}

/* static */ NPIdentifier
IdentifierCache::MakeOrphan(const NPUTF8* name)
{
	RETURN_VALUE_IF_NULL(name, NULL);

	OpAutoPtr<Identifier> id(OP_NEW(Identifier, ()));
	RETURN_VALUE_IF_NULL(id.get(), NULL);

	id->browser_identifier = 0;
	id->is_string = true;
	id->value.string = op_strdup(name);
	RETURN_VALUE_IF_NULL(id->value.string, NULL);

	return id.release();
}

/* static */ NPIdentifier
IdentifierCache::MakeOrphan(uint32_t name)
{
	Identifier* id = OP_NEW(Identifier, ());
	RETURN_VALUE_IF_NULL(id, NULL);

	id->browser_identifier = 0;
	id->is_string = false;
	id->value.integer = name;

	return id;
}

/* static */ bool
IdentifierCache::IsOrphan(NPIdentifier identifier)
{
	return GetBrowserIdentifier(identifier) == 0;
}

OP_STATUS
IdentifierCache::Adopt(NPIdentifier identifier)
{
	Identifier* id = static_cast<Identifier*>(identifier);
	return m_value_identifier_map.Insert(*id, id);
}

#endif // NS4P_COMPONENT_PLUGINS
