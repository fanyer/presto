/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef SCOPE_PROPERTY_FILTER_H
#define SCOPE_PROPERTY_FILTER_H

#ifdef SCOPE_ECMASCRIPT_DEBUGGER

#include "modules/ecmascript_utils/esdebugger.h"

/**
 * Property filters are used to exclude some properties when
 * examining an object.
 *
 * This class contains a collections of filters. There can be
 * one filter for each prototype, such as HTMLHtmlElementPrototype.
 *
 * Filtering works by looking at the name of the property. If
 * that name exists, then the property can be filtered based
 * on any of these conditions:
 *
 *  - Always filter the property, regardless of its value.
 *  - Filter if the value is an object, or nil.
 *  - Filter if the value is equal to a specified boolean.
 *  - Filter if the value is equal to a specified number.
 *  - Filter if the value is equal to a specified string.
 *
 * If the property name does not exist, the value will not be
 * filtered.
 */
class ES_ScopePropertyFilters
	: public ES_DebugPropertyFilters
{
public:

	/**
	 * Represents a 'rule' in the filter. Each value passing through
	 * the filter will be subjected to these rules.
	 */
	class Value
	{
	public:

		/**
		 * If type is a string, the destructor will op_free it.
		 */
		~Value();

		union
		{
			bool boolean;
			double number;
			uni_char *string;
		} value;

		/**
		 * VALUE_UNDEFINED,
		 * VALUE_NULL,
		 * VALUE_BOOLEAN,
		 * VALUE_NUMBER,
		 * VALUE_STRING,
		 * VALUE_OBJECT,
		 * VALUE_STRING_WITH_LENGTH
		 */
		ES_Value_Type type;

		/**
		 * Set this to true if the property should be filtered
		 * regardless of value.
		 */
		BOOL any;
	};

	/**
	 * This is a filter for a specific prototype. There will be
	 * one instance of this class for each prototype we want to
	 * apply filtering to.
	 */
	class Filter
		: public ES_PropertyFilter
	{
	public:

		/**
		 * Destructor. Frees all keys.
		 */
		virtual ~Filter();

		// From ES_PropertyFilter
		virtual BOOL Exclude(const uni_char *name, const ES_Value &value);

		/**
		 * Adds a property to exclude. The property will be excluded
		 * if the property value is equal to the specified number.
		 *
		 * @param key The name of the property to exclude.
		 * @param number The filter condition.
		 * @return OpStatus::OK; OpStatus::ERR if the key already exists; or
		 *         OpStatus::ERR_NO_MEMORY.
		 */
		OP_STATUS Add(const uni_char *key, double number);

		/**
		 * Adds a property to exclude. The property will be excluded
		 * if the property value is equal to the specified string.
		 *
		 * @param key The name of the property to exclude.
		 * @param string The filter condition.
		 * @return OpStatus::OK; OpStatus::ERR if the key already exists; or
		 *         OpStatus::ERR_NO_MEMORY.
		 */
		OP_STATUS Add(const uni_char *key, const uni_char *string);

		/**
		 * Adds a property to exclude. The property will be excluded
		 * if the property value is equal to the specified boolean.
		 *
		 * @param key The name of the property to exclude.
		 * @param boolean The filter condition.
		 * @return OpStatus::OK; OpStatus::ERR if the key already exists; or
		 *         OpStatus::ERR_NO_MEMORY.
		 */
		OP_STATUS Add(const uni_char *key, BOOL boolean);

		/**
		 * Adds a property to exclude. The property will be excluded
		 * if the property value is equal to the specified boolean.
		 *
		 * @param key The name of the property to exclude.
		 * @param type Filter only if the property value is of this type.
		 * @return OpStatus::OK; OpStatus::ERR if the key already exists; or
		 *         OpStatus::ERR_NO_MEMORY.
		 */
		OP_STATUS Add(const uni_char *key, ES_Value_Type type);

		/**
		 * Adds a property to exclude. The property will
		 * be excluded regardless of the value.
		 *
		 * @param key The name of the property to exclude
		 * @return OpStatus::OK; OpStatus::ERR if the key already exists; or
		 *         OpStatus::ERR_NO_MEMORY.
		 */
		OP_STATUS Add(const uni_char *key);

	private:

		/**
		 * Adds a key-value pair to the array, or cleans up in case
		 * of errors. If this function fails, 'v' will be freed.
		 *
		 * @param key The key to add.
		 * @param v The value, allocated by the caller.
		 * @return OpStatus::OK; OpStatus::ERR if the key already exists; or
		 *         OpStatus::ERR_NO_MEMORY.
		 */
		OP_STATUS AddValue(const uni_char *key, Value *v);

	private:

		// Contains the values that should be filtered.
		OpAutoStringHashTable<Value> values;

	};

	/**
	 * Destructor. Frees all keys.
	 */
	virtual ~ES_ScopePropertyFilters();

	// From ES_DebugPropertyFilters
	ES_PropertyFilter *GetPropertyFilter(const char *classname);

	/**
	 * Add a property filter for a certain class.
	 *
	 * @param classname The name for which this filter should apply.
	 * @param [out] filter The filter that was just added. The filter is owned
	 *                     by this ES_ScopePropertyFilters, and must not be
	 *                     released by the caller.
	 * @return OpStatus::OK on success; OpStatus::ERR if a filter with that
	 *         name is already present; or OpStatus::ERR_NO_MEMORY,
	 */
	OP_STATUS AddPropertyFilter(const char *classname, Filter *&filter);

	/**
	 * Remove all filters.
	 */
	void Clear();

private:

	// Contains the filter. Each filter applies to a class, e.g. 'HTMLHtmlElementPrototype'.
	OpAutoString8HashTable<Filter> filters;
};

#endif // SCOPE_ECMASCRIPT_DEBUGGER

#endif // SCOPE_PROPERTY_FILTER_H
