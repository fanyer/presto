/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Opera test plugin.
 *
 * NPVariant helper classes.
 *
 * Copyright (C) 2011 Opera Software ASA.
 */

#ifndef VARIANTS_H
#define VARIANTS_H

#include <string.h>

#include "common.h"


extern NPNetscapeFuncs* g_browser;

/**
 * Custom NPVariant utility defines.
 */
#define NPVARIANT_IS_NUMBER(_v) (NPVARIANT_IS_INT32(_v) || NPVARIANT_IS_DOUBLE(_v))
#define NPVARIANT_GET_NUMBER(_v) (NPVARIANT_IS_INT32(_v) ? \
	_v.value.intValue : static_cast<int32_t>(_v.value.doubleValue))

/**
 * NPVariant wrappers intended to ease construction.
 *
 * Examples:
 *
 * 1. NPVariant var = *IntVariant(int_val);
 * 2. NPN_InvokeDefault(instance, object, &ObjectVariant(arg_obj), 1);
 */
struct Variant
{
	NPVariant variant;
	NPVariant& operator*() { return variant; }
	NPVariant* operator&() { return &variant; }
};

struct ObjectVariant : public Variant
{
	/* Take void* argument to further simplify calling code. */
	ObjectVariant(void* object)
	{
		variant.type = NPVariantType_Object;
		variant.value.objectValue = static_cast<NPObject*>(object);
	}
};

struct StringVariant : public Variant
{
	StringVariant(NPString* string)
	{
		variant.type = NPVariantType_String;
		variant.value.stringValue = *string;
	}

	StringVariant(const char* string)
	{
		if (string)
		{
			variant.type = NPVariantType_String;
			variant.value.stringValue.UTF8Characters = string;
			variant.value.stringValue.UTF8Length = static_cast<uint32_t>(strlen(string));
		}
		else
			variant.type = NPVariantType_Null;
	}
};

struct IntVariant : public Variant
{
	IntVariant(int64_t value)
	{
		variant.type = NPVariantType_Int32;
		variant.value.intValue = static_cast<int32_t>(value);
	}
};

struct DoubleVariant : public Variant
{
	DoubleVariant(double value)
	{
		variant.type = NPVariantType_Double;
		variant.value.doubleValue = value;
	}
};

struct BoolVariant : public Variant
{
	BoolVariant(bool value)
	{
		variant.type = NPVariantType_Bool;
		variant.value.boolValue = value;
	}
};

struct VoidVariant : public Variant
{
	VoidVariant()
	{
		variant.type = NPVariantType_Void;
	}
};

struct AutoVariant : public Variant
{
	AutoVariant(NPIdentifier name)
	{
		if (g_browser->identifierisstring(name))
			variant = *StringVariant(g_browser->utf8fromidentifier(name));
		else
			variant = *IntVariant(g_browser->intfromidentifier(name));
	}
};

/**
 * Auto release for NPObjects and NPVariants.
 */
class AutoReleaseObject
{
public:
	AutoReleaseObject(NPObject* object = 0)
		: object(object)
	{
	}

	~AutoReleaseObject()
	{
		if (object)
			g_browser->releaseobject(object);
	}

	NPObject* Release()
	{
		NPObject* ret = object;
		object = 0;
		return ret;
	}

	NPObject* operator*() { return object; }

private:
	NPObject* object;
};

#endif // VARIANTS_H
