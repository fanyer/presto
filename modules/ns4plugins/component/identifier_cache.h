/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef NS4P_IDENTIFIER_CACHE_H
#define NS4P_IDENTIFIER_CACHE_H

#ifdef NS4P_COMPONENT_PLUGINS

#include "modules/otl/map.h"
#include "modules/ns4plugins/src/plug-inc/npapi.h"
#include "modules/ns4plugins/src/plug-inc/npruntime.h"
#include "modules/ns4plugins/src/generated/g_message_ns4plugins_messages.h"

class IdentifierCache
{
public:
	typedef OpNs4pluginsMessages_MessageSet::PluginIdentifier PluginIdentifier;

	/**
	 * Bind a browser NPIdentifier to a local identifier.
	 *
	 * If the identifier is known, the previous binding will be returned.
	 *
	 * @param identifier Browser side identifier object.
	 * @param string The string being bound, if any.
	 *
	 * @return Local identifier or NULL on OOM.
	 */
	NPIdentifier Bind(const PluginIdentifier& identifier, const NPUTF8* string = NULL);

	/**
	 * Look up cached identifier based on value.
	 *
	 * @param value The value to perform the lookup based on.
	 *
	 * @return Local identifier, or NULL if none found.
	 */
	NPIdentifier Lookup(int value);

	/**
	 * Look up cached identifier based on value.
	 *
	 * @param value The value to perform the lookup based on.
	 *
	 * @return Local identifier, or NULL if none found.
	 */
	NPIdentifier Lookup(const NPUTF8* value);

	/**
	 * Retrieve browser identifier for the same value.
	 *
	 * @param identifier Local identifier whose value we desire a browser identifier for.
	 *
	 * @return Browser identifier.
	 */
	static UINT64 GetBrowserIdentifier(NPIdentifier identifier);

	/**
	 * Check if identifier is a string.
	 *
	 * @param identifier Local identifier to classify.
	 *
	 * @return True if identifier value is a string, otherwise false.
	 */
	static bool IsString(NPIdentifier identifier);

	/**
	 * Retrieve integer from an identifier.
	 *
	 * @param identifier Identifier whose integer we wish to retrieve.
	 *
	 * @return Integer, always valid if the identifier is.
	 */
	static int32_t GetInteger(NPIdentifier identifier);

	/**
	 * Update identifier integer.
	 *
	 * @param identifier Identifier whose integer value we wish to update.
	 * @param integer Integer value to set.
	 */
	void SetInteger(NPIdentifier identifier, int32_t integer);

	/**
	 * Retrieve string from an identifier.
	 *
	 * If the string is not cached, it must be manually fetched by the caller, and
	 * updated using SetString().
	 *
	 * @param identifier Identifier whose string we wish to retrieve.
	 *
	 * @return Pointer to string content, or NULL if the string is not cached.
	 */
	static NPUTF8* GetString(NPIdentifier identifier);

	/**
	 * Retrieve string from an identifier.
	 *
	 * If the string is not cached, it must be manually fetched by the caller, and
	 * updated using SetString().
	 *
	 * @param identifier Identifier whose string we wish to retrieve.
	 *
	 * @return Newly NPN_MemAlloc-allocated copy of string contents.
	 */
	static NPUTF8* GetStringCopy(NPIdentifier identifier);

	/**
	 * Update identifier string.
	 *
	 * @param identifier Identifier whose string we wish to update.
	 * @param string String value to set.
	 */
	void SetString(NPIdentifier identifier, const NPUTF8* string);

	/**
	 * Unbind an identifier.
	 *
	 * @param identifier Browser identifier to unbind local copy of.
	 */
	void Unbind(UINT64 identifier);

	/**
	 * Remove all identifiers.
	 */
	void Clear();

	/**
	 * Create an orphan identifier.
	 *
	 * Used on third party threads where we have no access to the global identifier
	 * cache. An orphan identifier is recognized by the browser identifier zero.
	 *
	 * When such an identifier encounters the plug-in thread, a browser identifier
	 * will be allocated and the identifier will be an orphan no more.
	 *
	 * @param name Identifier name.
	 *
	 * @return New orphan NPIdentifier, or NULL on OOM.
	 */
	static NPIdentifier MakeOrphan(const NPUTF8* name);
	static NPIdentifier MakeOrphan(uint32_t name);

	/**
	 * Check if an identifier is an orphan.
	 */
	static bool IsOrphan(NPIdentifier identifier);

	/**
	 * Insert an orphan identifier into the cache map.
	 *
	 * @param identifier Orphan.
	 *
	 * @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM.
	 */
	OP_STATUS Adopt(NPIdentifier identifier);

protected:
	/** Local NPAPI Identifier representation. */
	struct Identifier
	{
		UINT64 browser_identifier;
		bool is_string;

		union
		{
			int32_t integer;
			NPUTF8* string;
		} value;

		/* OtlMap requires key types to implement total ordering. */
		bool operator<(const Identifier& o) const;
	};

	/** NPIdentifier -> Identifier map. See Bind(). */
	typedef OtlMap<UINT64, Identifier*> IdentifierMap;
	IdentifierMap m_identifier_map;

	/** <Int|String> -> Identifier map. For local caching purposes. */
	typedef OtlMap<Identifier, Identifier*> ValueIdentifierMap;
	ValueIdentifierMap m_value_identifier_map;
};

#endif // NS4P_COMPONENT_PLUGINS

#endif // NS4P_IDENTIFIER_CACHE_H
