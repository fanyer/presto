/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SCOPE_ECMASCRIPT_DEBUGGER

#include "modules/scope/src/scope_property_filter.h"

static void
FreeKey(const void *key, void *value)
{
	op_free((uni_char *)key);
}

/**
 * Utility class for anchoring string duplicates on the stack.
 */
template<typename T>
class OpScopeAutoDup
{
public:

	OpScopeAutoDup(T *s) : str(s) { };

	~OpScopeAutoDup() { op_free(str); }

	T *Get() const { return str; }

	T *Release()
	{
		T *tmp = str;
		str = NULL;
		return tmp;
	}

private:
	T *str;
};

ES_ScopePropertyFilters::Value::~Value()
{
	if (type == VALUE_STRING)
		op_free(value.string);
}

ES_ScopePropertyFilters::Filter::~Filter()
{
	values.ForEach(FreeKey);
}

BOOL
ES_ScopePropertyFilters::Filter::Exclude(const uni_char *name, const ES_Value &value)
{
	Value * v;

	if (OpStatus::IsSuccess(values.GetData(name, &v)))
	{
		if (v->any)
			return TRUE;
		else if (v->type != value.type)
			return FALSE;

		switch(value.type)
		{
		case VALUE_STRING:
			return (uni_strcmp(v->value.string, value.value.string) == 0);
		case VALUE_BOOLEAN:
			return (v->value.boolean == value.value.boolean);
		case VALUE_NUMBER:
			return (v->value.number == value.value.number);
		default: // On NULL, UNDEFINED and OBJECT: filter regardless of value.
			return TRUE;
		}
	}

	return FALSE;
}

OP_STATUS
ES_ScopePropertyFilters::Filter::Add(const uni_char *key, double number)
{
	OpAutoPtr<Value> v(OP_NEW(Value, ()));
	RETURN_OOM_IF_NULL(v.get());

	v->any = FALSE;
	v->type = VALUE_NUMBER;
	v->value.number = number;

	RETURN_IF_ERROR(AddValue(key, v.get()));

	v.release();
	return OpStatus::OK;
}

OP_STATUS
ES_ScopePropertyFilters::Filter::Add(const uni_char *key, const uni_char *string)
{
	OpAutoPtr<Value> v(OP_NEW(Value, ()));
	RETURN_OOM_IF_NULL(v.get());

	v->any = FALSE;
	v->type = VALUE_STRING;
	v->value.string = uni_strdup(string);

	RETURN_OOM_IF_NULL(v->value.string);
	RETURN_IF_ERROR(AddValue(key, v.get()));

	v.release();
	return OpStatus::OK;
}

OP_STATUS
ES_ScopePropertyFilters::Filter::Add(const uni_char *key, BOOL boolean)
{
	OpAutoPtr<Value> v(OP_NEW(Value, ()));
	RETURN_OOM_IF_NULL(v.get());

	v->any = FALSE;
	v->type = VALUE_BOOLEAN;
	v->value.boolean = !!boolean;

	RETURN_IF_ERROR(AddValue(key, v.get()));

	v.release();
	return OpStatus::OK;
}

OP_STATUS
ES_ScopePropertyFilters::Filter::Add(const uni_char *key, ES_Value_Type type)
{
	OpAutoPtr<Value> v(OP_NEW(Value, ()));
	RETURN_OOM_IF_NULL(v.get());

	v->any = FALSE;
	v->type = type;

	RETURN_IF_ERROR(AddValue(key, v.get()));

	v.release();
	return OpStatus::OK;
}

OP_STATUS
ES_ScopePropertyFilters::Filter::Add(const uni_char *key)
{
	OpAutoPtr<Value> v(OP_NEW(Value, ()));
	RETURN_OOM_IF_NULL(v.get());

	v->any = TRUE;

	RETURN_IF_ERROR(AddValue(key, v.get()));

	v.release();
	return OpStatus::OK;
}

OP_STATUS
ES_ScopePropertyFilters::Filter::AddValue(const uni_char *key, Value *v)
{
	OP_ASSERT(key);
	OP_ASSERT(v);

	OpScopeAutoDup<uni_char> key_dup(uni_strdup(key));
	RETURN_OOM_IF_NULL(key_dup.Get());

	RETURN_IF_ERROR(values.Add(key_dup.Get(), v));

	key_dup.Release();

	return OpStatus::OK;
}

ES_ScopePropertyFilters::~ES_ScopePropertyFilters()
{
	Clear();
}

OP_STATUS
ES_ScopePropertyFilters::AddPropertyFilter(const char *classname, Filter *&filter)
{
	OpAutoPtr<Filter> f(OP_NEW(Filter, ()));
	RETURN_OOM_IF_NULL(f.get());

	OpScopeAutoDup<char> dup(op_strdup(classname));
	RETURN_OOM_IF_NULL(dup.Get());

	RETURN_IF_ERROR(filters.Add(dup.Get(), f.get()));

	// Both are owned by 'filters' now.
	dup.Release();
	filter = f.release();

	return OpStatus::OK;
}

ES_PropertyFilter *
ES_ScopePropertyFilters::GetPropertyFilter(const char *classname)
{
	ES_ScopePropertyFilters::Filter *filter;

	if(OpStatus::IsSuccess(filters.GetData(classname, &filter)))
		return filter;

	return NULL;
}

void
ES_ScopePropertyFilters::Clear()
{
	filters.ForEach(FreeKey);
	filters.DeleteAll();
}

#endif // SCOPE_ECMASCRIPT_DEBUGGER
