/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
*/

#include "core/pch.h"
#include "modules/style/css_decl.h"
#include "modules/style/css.h"
#include "modules/logdoc/htm_lex.h"
#include "modules/display/vis_dev.h"
#include "modules/doc/frm_doc.h"
#include "modules/layout/layoutprops.h"

BOOL CSS_type_decl::IsEqual(const CSS_decl* decl) const
{
    if (decl->GetDeclType() == CSS_DECL_TYPE)
        return TypeValue() == decl->TypeValue();
    return FALSE;
}

BOOL CSS_array_decl::IsEqual(const CSS_decl* decl) const
{
    if (decl->GetDeclType() == CSS_DECL_ARRAY)
        return FALSE; // TODO: fixme stighal
    return FALSE;
}

BOOL CSS_gen_array_decl::IsEqual(const CSS_decl* decl) const
{
	if (decl->GetDeclType() == CSS_DECL_GEN_ARRAY)
	{
		CSS_gen_array_decl* other = (CSS_gen_array_decl*) decl;

		const CSS_generic_value* gen_values = GenArrayValue();
		const CSS_generic_value* other_gen_values = other->GenArrayValue();

		if (layer_count != other->layer_count)
			return FALSE;

		if (array_len != other->array_len)
			return FALSE;

		for (int i = 0; i < array_len; i ++)
		{
			if (gen_values[i].value_type != other_gen_values[i].value_type)
				return FALSE;

			if (gen_values[i].value_type == CSS_STRING_LITERAL)
			{
				if (uni_strcmp(gen_values[i].value.string, other_gen_values[i].value.string))
					return FALSE;
			}
			else
				if (op_memcmp(&gen_values[i].value, &other_gen_values[i].value, sizeof(gen_values[i].value)))
					return FALSE;
		}

		return TRUE;
	}

	return FALSE;
}

#ifdef CSS_TRANSITIONS
BOOL
CSS_heap_gen_array_decl::Transition(CSS_decl* from, CSS_decl* to, float percentage)
{
	if (!from || !to || from->ArrayValueLen() != ArrayValueLen() || from->ArrayValueLen() != to->ArrayValueLen())
	{
		OP_ASSERT(!"transitions between generic arrays must happen between arrays of equal length.");
		return FALSE;
	}

	BOOL changed = FALSE;
	int count = ArrayValueLen();
	const CSS_generic_value* from_arr = from->GenArrayValue();
	const CSS_generic_value* to_arr = to->GenArrayValue();

	for (int i=0; i<count; i++)
	{
		short val_type = gen_values[i].GetValueType();
		if (val_type == CSS_PX || val_type == CSS_PERCENTAGE)
		{
			float old_val = gen_values[i].GetReal();
			float new_val = from_arr[i].GetReal() + (to_arr[i].GetReal() - from_arr[i].GetReal())*percentage;
			if (old_val != new_val)
			{
				gen_values[i].SetReal(new_val);
				changed = TRUE;
			}
		}
		else if (val_type == CSS_DECL_COLOR)
		{
			COLORREF old_val = gen_values[i].value.color;
			COLORREF new_val = InterpolatePremultipliedRGBA(from_arr[i].value.color, to_arr[i].value.color, percentage);
			if (old_val != new_val)
			{
				gen_values[i].value.color = new_val;
				changed = TRUE;
			}
		}
	}

	return changed;
}
#endif // CSS_TRANSITIONS

BOOL CSS_long_decl::IsEqual(const CSS_decl* decl) const
{
    if (decl->GetDeclType() == CSS_DECL_LONG)
        return LongValue() == decl->LongValue();
    return FALSE;
}

BOOL CSS_color_decl::IsEqual(const CSS_decl* decl) const
{
    if (decl->GetDeclType() == CSS_DECL_COLOR)
        return LongValue() == decl->LongValue();
    return FALSE;
}

BOOL CSS_string_decl::IsEqual(const CSS_decl* decl) const
{
    if (decl->GetDeclType() == CSS_DECL_STRING && ((CSS_string_decl*)decl)->GetStringType() == GetStringType())
    {
        const uni_char* str1 = StringValue();
        const uni_char* str2 = decl->StringValue();
        if (str1 && str2)
            return uni_strcmp(str1, str2) == 0;
        else if (!str1 && !str2)
            return TRUE;
    }
    return FALSE;
}

BOOL CSS_number_decl::IsEqual(const CSS_decl* decl) const
{
    if (decl->GetDeclType() == CSS_DECL_NUMBER)
        return (GetNumberValue(0) == decl->GetNumberValue(0)) && (GetValueType(0) == decl->GetValueType(0));
    return FALSE;
}

BOOL CSS_number2_decl::IsEqual(const CSS_decl* decl) const
{
    if (decl->GetDeclType() == CSS_DECL_NUMBER2)
        return (GetNumberValue(0) == decl->GetNumberValue(0)) && (GetValueType(0) == decl->GetValueType(0))
            && (GetNumberValue(1) == decl->GetNumberValue(1)) && (GetValueType(1) == decl->GetValueType(1));
    return FALSE;
}

BOOL CSS_number4_decl::IsEqual(const CSS_decl* decl) const
{
    if (decl->GetDeclType() == CSS_DECL_NUMBER4)
    {
        for (int i = 0; i < 4; i++)
        {
            if (GetNumberValue(i) != decl->GetNumberValue(i)
                || GetValueType(i) != decl->GetValueType(i))
                return FALSE;
        }
        return TRUE;
    }
    return FALSE;
}

CSS_type_decl::CSS_type_decl(short prop, CSSValue val) : CSS_decl(prop)
{
	OP_ASSERT(val != CSS_VALUE_UNKNOWN);
	info.data.extra.type_value = val;
}

CSS_array_decl::~CSS_array_decl()
{
	OP_DELETEA(value);
}

OP_STATUS CSS_array_decl::Construct(short* val_array, int val_array_len)
{
	value = OP_NEWA(short, val_array_len);
	if (value)
	{
		array_len = val_array_len;
		op_memcpy(value, val_array, sizeof(short)*val_array_len);
		return OpStatus::OK;
	}
	else
		return OpStatus::ERR_NO_MEMORY;
}

CSS_array_decl* CSS_array_decl::Create(short prop, short* val_array, int val_array_len)
{
	CSS_array_decl* css_decl = OP_NEW(CSS_array_decl, (prop));
	if (!css_decl)
		return NULL;
	OP_STATUS err = css_decl->Construct(val_array, val_array_len);
	if (OpStatus::IsError(err))
	{
		OP_DELETE(css_decl);
		return NULL;
	}
	return css_decl;
}

void
CSS_generic_value_list::PushIdentL(CSSValue value)
{
	CSS_generic_value_item* gen_value = OP_NEW_L(CSS_generic_value_item, ());
	gen_value->value.SetValueType(CSS_IDENT);
	gen_value->value.SetType(value);
	gen_value->Into(this);
}

void
CSS_generic_value_list::PushIntegerL(CSSValueType value_type, int value)
{
	CSS_generic_value_item* gen_value = OP_NEW_L(CSS_generic_value_item, ());
	gen_value->value.SetValueType(value_type);
	gen_value->value.SetNumber(value);
	gen_value->Into(this);
}

void
CSS_generic_value_list::PushValueTypeL(CSSValueType value_type)
{
	CSS_generic_value_item* gen_value = OP_NEW_L(CSS_generic_value_item, ());
	gen_value->value.SetValueType(value_type);
	gen_value->Into(this);
}

void
CSS_generic_value_list::PushStringL(CSSValueType value_type, uni_char* str)
{
	CSS_generic_value_item* gen_value = OP_NEW_L(CSS_generic_value_item, ());
	gen_value->value.SetValueType(value_type);
	gen_value->value.value.string = str;
	gen_value->Into(this);
}

#ifdef CSS_GRADIENT_SUPPORT
void
CSS_generic_value_list::PushGradientL(CSSValueType value_type, CSS_Gradient* gradient)
{
	CSS_generic_value_item* gen_value = OP_NEW_L(CSS_generic_value_item, ());
	gen_value->value.SetValueType(value_type);
	gen_value->value.value.gradient = gradient;
	gen_value->Into(this);
}
#endif // CSS_GRADIENT_SUPPORT

void
CSS_generic_value_list::PushNumberL(CSSValueType unit, float real_value)
{
	CSS_generic_value_item* gen_value = OP_NEW_L(CSS_generic_value_item, ());
	gen_value->value.SetValueType(unit);
	gen_value->value.SetReal(real_value);
	gen_value->Into(this);
}

void
CSS_generic_value_list::PushGenericValueL(const CSS_generic_value& value)
{
	CSS_generic_value_item* gen_value = OP_NEW_L(CSS_generic_value_item, ());
	gen_value->value = value;
	gen_value->Into(this);
}

CSS_gen_array_decl::CSS_gen_array_decl(short prop, int val_array_len) :
	CSS_decl(prop),
	array_len(val_array_len),
	layer_count(0)
{
	info.data.prefetch = 1;
}

OP_STATUS CSS_gen_array_decl::PrefetchURLs(FramesDocument* doc)
{
	if (info.data.prefetch)
	{
		info.data.prefetch = 0;

		const CSS_generic_value* gen_values = GenArrayValue();

		for (int i = 0; i < array_len; i++)
		{
			if (gen_values[i].GetValueType() == CSS_FUNCTION_URL)
			{
				URL url = g_url_api->GetURL(gen_values[i].GetString());
				if (OpStatus::IsMemoryError(doc->LoadInline(&url, doc->GetDocRoot(), GENERIC_INLINE)))
					return OpStatus::ERR_NO_MEMORY;
			}
		}
	}

	return OpStatus::OK;
}

CSS_decl* CSS_gen_array_decl::CreateCopy() const
{
	return CSS_copy_gen_array_decl::Copy(GetProperty(), this);
}

CSS_stack_gen_array_decl::CSS_stack_gen_array_decl(short prop)
	: CSS_gen_array_decl(prop, 0),
	  gen_values(NULL)
{
}

void CSS_stack_gen_array_decl::Set(const CSS_generic_value* val_array, int val_array_len)
{
	gen_values = val_array;
	array_len = val_array_len;
}

CSS_gen_array_decl* CSS_heap_gen_array_decl::Create(short prop, CSS_generic_value* val_array, int val_array_len)
{
	if (!val_array)
		return NULL;

	CSS_heap_gen_array_decl* new_array_decl = OP_NEW(CSS_heap_gen_array_decl, (prop));

	if (!new_array_decl)
	{
		OP_DELETEA(val_array);
		return NULL;
	}

	new_array_decl->gen_values = val_array;

	new_array_decl->array_len = val_array_len;
	return new_array_decl;
}

CSS_heap_gen_array_decl::~CSS_heap_gen_array_decl()
{
	if (gen_values)
	{
		for (int i=0; i<array_len; i++)
		{
			switch (gen_values[i].value_type)
			{
			case CSS_STRING_LITERAL:
			case CSS_FUNCTION_ATTR:
			case CSS_FUNCTION_COUNTER:
			case CSS_FUNCTION_COUNTERS:
			case CSS_FUNCTION_URL:
			case CSS_HASH:
#ifdef SKIN_SUPPORT
			case CSS_FUNCTION_SKIN:
#endif // SKIN_SUPPORT
			case CSS_FUNCTION_LOCAL:
				OP_DELETEA(gen_values[i].value.string);
				break;

#ifdef CSS_GRADIENT_SUPPORT
			case CSS_FUNCTION_LINEAR_GRADIENT:
			case CSS_FUNCTION_WEBKIT_LINEAR_GRADIENT:
			case CSS_FUNCTION_O_LINEAR_GRADIENT:
			case CSS_FUNCTION_REPEATING_LINEAR_GRADIENT:
			case CSS_FUNCTION_RADIAL_GRADIENT:
			case CSS_FUNCTION_REPEATING_RADIAL_GRADIENT:
				OP_DELETE(gen_values[i].value.gradient);
				break;
#endif // CSS_GRADIENT_SUPPORT
			}
		}
		OP_DELETEA(gen_values);
	}
}

CSS_copy_gen_array_decl::CSS_copy_gen_array_decl(short prop)
	: CSS_heap_gen_array_decl(prop)
{
}

CSS_copy_gen_array_decl* CSS_copy_gen_array_decl::Create(short prop, const CSS_generic_value* val_array, int val_array_len)
{
	OP_STATUS retval = OpStatus::OK;

	CSS_generic_value* gen_values = OP_NEWA(CSS_generic_value, val_array_len);
	if (gen_values)
	{
		op_memcpy(gen_values, val_array, sizeof(CSS_generic_value)*val_array_len);

		for (int i=0; i<val_array_len; i++)
		{
			switch (val_array[i].value_type)
			{
			case CSS_STRING_LITERAL:
			case CSS_FUNCTION_ATTR:
			case CSS_FUNCTION_COUNTER:
			case CSS_FUNCTION_COUNTERS:
			case CSS_FUNCTION_URL:
			case CSS_HASH:
#ifdef SKIN_SUPPORT
			case CSS_FUNCTION_SKIN:
#endif // SKIN_SUPPORT
			case CSS_FUNCTION_LOCAL:
				if (val_array[i].value.string)
				{
					if (OpStatus::IsSuccess(retval))
					{
						gen_values[i].value.string = OP_NEWA(uni_char, (uni_strlen(val_array[i].value.string)+1));
						if (gen_values[i].value.string)
						{
							uni_strcpy(gen_values[i].value.string, val_array[i].value.string);
						}
						else
							retval = OpStatus::ERR_NO_MEMORY;
					}
					else
						gen_values[i].value.string = NULL;
				}
				break;
#ifdef CSS_GRADIENT_SUPPORT
			case CSS_FUNCTION_LINEAR_GRADIENT:
			case CSS_FUNCTION_WEBKIT_LINEAR_GRADIENT:
			case CSS_FUNCTION_O_LINEAR_GRADIENT:
			case CSS_FUNCTION_REPEATING_LINEAR_GRADIENT:
			case CSS_FUNCTION_RADIAL_GRADIENT:
			case CSS_FUNCTION_REPEATING_RADIAL_GRADIENT:
				if (val_array[i].value.gradient)
				{
					if (OpStatus::IsSuccess(retval))
					{
						gen_values[i].value.gradient = val_array[i].value.gradient->Copy();
						if (!gen_values[i].value.gradient)
							retval = OpStatus::ERR_NO_MEMORY;
						else
							retval = OpStatus::OK;
					}
					else
						gen_values[i].value.gradient = NULL;
				}
				break;
#endif // CSS_GRADIENT_SUPPORT
			}

		}
	}
	else
		retval = OpStatus::ERR_NO_MEMORY;

	if (OpStatus::IsSuccess(retval))
	{
		CSS_copy_gen_array_decl* new_decl = OP_NEW(CSS_copy_gen_array_decl, (prop));
		if (new_decl)
		{
			new_decl->gen_values = gen_values;
			new_decl->array_len = val_array_len;
			return new_decl;
		}
	}

	OP_DELETEA(gen_values);
	return NULL;
}

CSS_copy_gen_array_decl*
CSS_copy_gen_array_decl::Copy(short prop, const CSS_gen_array_decl* src)
{
	CSS_copy_gen_array_decl* ret_val = Create(prop, src->GenArrayValue(), src->ArrayValueLen());
	if (!ret_val)
		return NULL;
	ret_val->SetLayerCount(src->GetLayerCount());
	return ret_val;
}

CSS_copy_gen_array_decl* CSS_copy_gen_array_decl::Create(short prop, const CSS_generic_value_list& val_list)
{
	int val_list_len = val_list.Cardinal();
	CSS_generic_value* gen_values = OP_NEWA(CSS_generic_value, (val_list_len));
	if (!gen_values)
		return NULL;

	int i = 0;
	OP_STATUS retval = OpStatus::OK;
	CSS_generic_value_item* iter = val_list.First();
	while (iter)
	{
		gen_values[i] = iter->value;

		switch (iter->value.value_type)
		{
		case CSS_STRING_LITERAL:
		case CSS_FUNCTION_ATTR:
		case CSS_FUNCTION_COUNTER:
		case CSS_FUNCTION_COUNTERS:
		case CSS_FUNCTION_URL:
		case CSS_HASH:
#ifdef SKIN_SUPPORT
		case CSS_FUNCTION_SKIN:
#endif // SKIN_SUPPORT
		case CSS_FUNCTION_LOCAL:
			if (iter->value.value.string)
			{
				if (OpStatus::IsSuccess(retval))
				{
					gen_values[i].value.string = OP_NEWA(uni_char, (uni_strlen(iter->value.value.string)+1));
					if (gen_values[i].value.string)
						uni_strcpy(gen_values[i].value.string, iter->value.value.string);
					else
						retval = OpStatus::ERR_NO_MEMORY;
				}
				else
					gen_values[i].value.string = NULL;
			}
			break;

#ifdef CSS_GRADIENT_SUPPORT
		case CSS_FUNCTION_LINEAR_GRADIENT:
		case CSS_FUNCTION_WEBKIT_LINEAR_GRADIENT:
		case CSS_FUNCTION_O_LINEAR_GRADIENT:
		case CSS_FUNCTION_REPEATING_LINEAR_GRADIENT:
		case CSS_FUNCTION_RADIAL_GRADIENT:
		case CSS_FUNCTION_REPEATING_RADIAL_GRADIENT:
			if (iter->value.value.gradient)
			{
				if (OpStatus::IsSuccess(retval))
				{
					gen_values[i].value.gradient = iter->value.value.gradient->Copy();
					if (!gen_values[i].value.gradient)
						retval = OpStatus::ERR_NO_MEMORY;
					else
						retval = OpStatus::OK;
				}
				else
					gen_values[i].value.gradient = NULL;
			}
#endif // CSS_GRADIENT_SUPPORT
		}

		iter = iter->Suc();
		i++;
	}

	CSS_copy_gen_array_decl* new_decl = OP_NEW(CSS_copy_gen_array_decl, (prop));
	if (!new_decl)
	{
		OP_DELETEA(gen_values);
		return NULL;
	}

	new_decl->gen_values = gen_values;
	new_decl->array_len = val_list_len;

	return new_decl;
}

CSS_long_decl::CSS_long_decl(short prop, long val) : CSS_decl(prop)
{
	value = val;
}

CSS_color_decl::CSS_color_decl(short prop, long val) : CSS_long_decl(prop, val)
{
	OP_ASSERT(val != -1);
}

CSS_string_decl::CSS_string_decl(short prop, StringType type, BOOL source_local, BOOL dont_delete) : CSS_decl(prop), value(0)
{
	packed.type = type;
	packed.source_local = source_local;
	packed.dont_delete = dont_delete;
	info.data.prefetch = (type == StringDeclUrl);
}

CSS_string_decl::CSS_string_decl(short prop, BOOL is_url) : CSS_decl(prop), value(0)
{
	packed.type = is_url ? StringDeclUrl : StringDeclString;
	packed.source_local = FALSE;
	packed.dont_delete = FALSE;
	info.data.prefetch = !!is_url;
}

CSS_string_decl::~CSS_string_decl()
{
	if (!packed.dont_delete)
		OP_DELETEA(const_cast<uni_char*>(value));
}

OP_STATUS CSS_string_decl::CopyAndSetString(const uni_char* val, int val_len)
{
	OP_ASSERT(!packed.dont_delete);

	if (value)
	{
		OP_DELETEA(const_cast<uni_char*>(value));
		value = NULL;
	}

	if (val && val_len)
	{
		uni_char* new_value = OP_NEWA(uni_char, val_len+1);
		if (new_value)
		{
			uni_strncpy(new_value, val, val_len);
			new_value[val_len] = 0;
			value = new_value;
		}
		else
			return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}

CSS_string_decl::CSS_string_decl(short prop, uni_char* val, StringType type, BOOL source_local) : CSS_decl(prop), value(val)
{
	packed.type = type;
	packed.source_local = source_local;
	packed.dont_delete = FALSE;
	info.data.prefetch = (type == StringDeclUrl);
}

CSS_decl* CSS_string_decl::CreateCopy() const
{
	CSS_string_decl* new_decl = OP_NEW_L(CSS_string_decl, (GetProperty(), GetStringType(), IsSourceLocal()));

	if (value)
		if (OpStatus::IsMemoryError(new_decl->CopyAndSetString(value, uni_strlen(value))))
		{
			OP_DELETE(new_decl);
			return NULL;
		}

	return new_decl;
}

OP_STATUS CSS_string_decl::PrefetchURLs(FramesDocument* doc)
{
	if (info.data.prefetch)
	{
		OP_ASSERT(packed.type == StringDeclUrl);
		info.data.prefetch = 0;
		URL url = g_url_api->GetURL(value);
		if (OpStatus::IsMemoryError(doc->LoadInline(&url, doc->GetDocRoot(), GENERIC_INLINE)))
			return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}

CSS_number_decl::CSS_number_decl(short prop, float val, int val_type) : CSS_decl(prop)
{
	value = val;
	info.data.extra.value_type = val_type;
	//value_type = val_type;
}

CSS_number2_decl::CSS_number2_decl(short prop, float val1, float val2, int val1_type, int val2_type) : CSS_number_decl(prop, val1, val1_type)
{
	value2 = val2;
	value2_type = val2_type;
}

CSS_number4_decl::CSS_number4_decl(short prop) : CSS_decl(prop)
{
	value[0] = value[1] = value[2] = value[3] = 0.0f;
	value_type[0] = value_type[1] = value_type[2] = value_type[3] = CSS_NUMBER;
}

CSS_decl* CSS_number4_decl::CreateCopy() const
{
	CSS_number4_decl* new_decl = OP_NEW(CSS_number4_decl, (GetProperty()));

	if (new_decl)
		for (int i=0; i<4; i++)
		{
			new_decl->value[i] = value[i];
			new_decl->value_type[i] = value_type[i];
		}

	return new_decl;
}

#ifdef CSS_TRANSFORMS

void CSS_transform_list::CSS_transform_item::SetFromAffineTransform(const AffineTransform& at)
{
	/* 'item' is column-major, AffineTransform is row-major. */

	value[0] = at[0]; value[1] = at[3]; value[2] = at[1];
	value[3] = at[4]; value[4] = at[2]; value[5] = at[5];

	for (int i=0; i<6;i++)
		value_type[i] = CSS_NUMBER;

	n_values = 6;
	type = CSS_VALUE_matrix;
}

BOOL CSS_transform_list::GetAffineTransform(AffineTransform& res) const
{
	res.LoadIdentity();

	const CSS_transform_list::CSS_transform_item* iter = First();

	while (iter)
	{
		AffineTransform tmp;

		switch(iter->type)
		{
		case CSS_VALUE_rotate:
			tmp.LoadRotate(iter->value[0]);
			break;
		case CSS_VALUE_scaleY:
			tmp.LoadScale(1, iter->value[0]);
			break;
		case CSS_VALUE_scaleX:
			tmp.LoadScale(iter->value[0], 1);
			break;
		case CSS_VALUE_scale:
			if (iter->n_values == 2)
				tmp.LoadScale(iter->value[0], iter->value[1]);
			else
				tmp.LoadScale(iter->value[0], iter->value[0]);
			break;
		case CSS_VALUE_translate:
		case CSS_VALUE_translateX:
		case CSS_VALUE_translateY:
		{
			float tx = 0;
			short txunit = CSS_PX;

			float ty = 0;
			short tyunit = CSS_PX;

			if (iter->type == CSS_VALUE_translateY)
			{
				ty = iter->value[0];
				tyunit = iter->value_type[0];
			}
			else
			{
				tx = iter->value[0];
				txunit = iter->value_type[0];

				if (iter->type == CSS_VALUE_translate && iter->n_values > 1)
				{
					ty = iter->value[1];
					tyunit = iter->value_type[1];
				}
			}

			if ((tx != 0 && txunit != CSS_PX) || (ty != 0 && tyunit != CSS_PX))
				return FALSE;

			tmp.LoadTranslate(tx, ty);
		}
		break;
		case CSS_VALUE_skew:
		case CSS_VALUE_skewX:
		case CSS_VALUE_skewY:
		{
			float sx = 0.0, sy = 0.0;

			if (iter->type == CSS_VALUE_skewY)
				sy = iter->value[0];
			else
			{
				sx = iter->value[0];

				if (iter->type == CSS_VALUE_skew && iter->n_values  > 1)
					sy = iter->value[1];
			}

			tmp.LoadSkew(sx, sy);
		}
		break;
		case CSS_VALUE_matrix:
			tmp.LoadValuesCM(iter->value[0], iter->value[1],
							 iter->value[2], iter->value[3],
							 iter->value[4], iter->value[5]);
			break;
		default:
			OP_ASSERT(!"Not reached");
		}

		res.PostMultiply(tmp);
		iter = iter->Suc();
	}

	return TRUE;
}

CSS_transform_list::CSS_transform_list(short prop) : CSS_decl(prop)
{
}

CSS_transform_list::~CSS_transform_list()
{
	values.Clear();
}

/* static */ CSS_transform_list*
CSS_transform_list::MakeFromAffineTransform(const AffineTransform& at)
{
	CSS_transform_list* ret_decl = OP_NEW(CSS_transform_list, (CSS_PROPERTY_transform));

	if (!ret_decl)
		return NULL;

	CSS_transform_item* new_item = OP_NEW(CSS_transform_item, ());
	if (!new_item)
	{
		OP_DELETE(ret_decl);
		return NULL;
	}

	new_item->SetFromAffineTransform(at);
	new_item->Into(&ret_decl->values);

	return ret_decl;
}

OP_STATUS
CSS_transform_list::StartTransformFunction(short operation_type)
{
	CSS_transform_item* item = OP_NEW(CSS_transform_item, ());
	if (!item)
		return OpStatus::ERR_NO_MEMORY;

	item->type = operation_type;
	item->n_values = 0;

	item->Into(&values);
	return OpStatus::OK;
}

void
CSS_transform_list::AppendValue(float value, short value_type)
{
	CSS_transform_item* item = (CSS_transform_item*)values.Last();

	/* Caller is responsible for starting a function before appending values. */

	OP_ASSERT(item);

	if (item->n_values < 6)
	{
		item->value[item->n_values] = value;
		item->value_type[item->n_values++] = value_type;
	}
}

/* virtual */ CSS_decl*
CSS_transform_list::CreateCopy() const
{
	CSS_transform_list* new_decl = OP_NEW(CSS_transform_list, (GetProperty()));

	if (!new_decl)
		return NULL;

	const CSS_transform_item* iter = First();
	while (iter)
	{
		CSS_transform_item* new_item = OP_NEW(CSS_transform_item, ());

		if (!new_item)
		{
			OP_DELETE(new_decl);
			return NULL;
		}

		for (int i=0; i<iter->n_values;i++)
		{
			new_item->value[i] = iter->value[i];
			new_item->value_type[i] = iter->value_type[i];
		}
		new_item->type = iter->type;
		new_item->n_values = iter->n_values;

		new_item->Into(&new_decl->values);

		iter = (CSS_transform_item*)iter->Suc();
	}

	return new_decl;
}

/* virtual */ BOOL
CSS_transform_list::IsEqual(const CSS_decl* decl) const
{
	if (decl->GetDeclType() != CSS_DECL_TRANSFORM_LIST)
		return FALSE;

	CSS_transform_list* other = (CSS_transform_list*)decl;

	const CSS_transform_item* iter1 = First();
	const CSS_transform_item* iter2 = other->First();

	while (iter1 && iter2)
	{
		if (iter1->n_values != iter2->n_values)
			return FALSE;

		if (iter1->type != iter2->type)
			return FALSE;

		for (int i=0; i<iter1->n_values;i++)
		{
			if (iter1->value[i] != iter2->value[i])
				return FALSE;

			if (iter1->value_type[i] != iter2->value_type[i])
				return FALSE;
		}

		iter1 = (CSS_transform_item*)iter1->Suc();
		iter2 = (CSS_transform_item*)iter2->Suc();
	}

	return iter1 == NULL && iter2 == NULL;
}

# ifdef CSS_TRANSITIONS

CSS_transform_list::CSS_transform_item*
CSS_transform_list::CSS_transform_item::MakeCompatibleZero(const CSS_transform_item* item)
{
	type = item->type;
	n_values = item->n_values;

	const float default_value = (type == CSS_VALUE_scale || type == CSS_VALUE_scaleX || type == CSS_VALUE_scaleY) ? float(1) : 0;
	for (int i=0;i<n_values;i++)
	{
		value[i] = default_value;
		value_type[i] = item->value_type[i];
	}

	if (type == CSS_VALUE_matrix)
		value[0] = value[3] = 1; // Set identity, zeros set above

	return this;
}

void
CSS_transform_list::CSS_transform_item::ResolveRelativeLengths(const CSSLengthResolver& length_resolver)
{
	if (type == CSS_VALUE_translateX ||
		type == CSS_VALUE_translateY ||
		type == CSS_VALUE_translate)
	{
		for (int i=0; i < n_values; i++)
			if (value_type[i] != CSS_PX && value_type[i] != CSS_PERCENTAGE)
			{
				value[i] = static_cast<float>(length_resolver.GetLengthInPixels(value[i], value_type[i]));
				value_type[i] = CSS_PX;
			}
	}
}

static BOOL IsCompatibleTransformFunction(short val1, short val2)
{
	switch (val1)
	{
	case CSS_VALUE_matrix:
		return val2 == CSS_VALUE_matrix;

	case CSS_VALUE_translate:
	case CSS_VALUE_translateX:
	case CSS_VALUE_translateY:
		switch(val2)
		{
		case CSS_VALUE_translate:
		case CSS_VALUE_translateX:
		case CSS_VALUE_translateY:
			return TRUE;
		default:
			return FALSE;
		}

	case CSS_VALUE_scale:
	case CSS_VALUE_scaleX:
	case CSS_VALUE_scaleY:
		switch(val2)
		{
		case CSS_VALUE_scale:
		case CSS_VALUE_scaleX:
		case CSS_VALUE_scaleY:
			return TRUE;
		default:
			return FALSE;
		}

	case CSS_VALUE_rotate:
		return val2 == CSS_VALUE_rotate;

	case CSS_VALUE_skew:
	case CSS_VALUE_skewX:
	case CSS_VALUE_skewY:
		switch(val2)
		{
		case CSS_VALUE_skew:
		case CSS_VALUE_skewX:
		case CSS_VALUE_skewY:
			return TRUE;
		default:
			return FALSE;
		}
	default:
		OP_ASSERT(!"Unrecognized transform type");
		return FALSE;
	}
}

void
CSS_transform_list::CSS_transform_item::ExpandFunction()
{
	if (n_values < 2)
	{
		int i = 1;
		switch (type)
		{
		case CSS_VALUE_translateY:
			i = 0;
			value[1] = value[0];
			value_type[1] = value_type[0];
		case CSS_VALUE_translateX:
		case CSS_VALUE_translate:
			value[i] = 0;
			value_type[i] = CSS_PX;
			type = CSS_VALUE_translate;
			break;

		case CSS_VALUE_skewY:
			i = 0;
			value[1] = value[0];
			value_type[1] = value_type[0];
		case CSS_VALUE_skewX:
		case CSS_VALUE_skew:
			value[i] = 0;
			value_type[i] = CSS_NUMBER;
			type = CSS_VALUE_skew;
			break;

		case CSS_VALUE_scaleY:
			i = 0;
			value[1] = value[0];
			value_type[1] = value_type[0];
		case CSS_VALUE_scaleX:
			value[i] = 0;
			value_type[i] = CSS_NUMBER;
			type = CSS_VALUE_scale;
			break;
		case CSS_VALUE_scale:
			value[1] = value[0];
			value_type[1] = value_type[0];
			break;
		}
		n_values = 2;
	}
}

/* static */ BOOL
CSS_transform_list::CSS_transform_item::MakeCompatible(CSS_transform_item& from, CSS_transform_item& to)
{
	if (!IsCompatibleTransformFunction(from.type, to.type))
		return FALSE;

	from.ExpandFunction();
	to.ExpandFunction();

	if (from.type == CSS_VALUE_translate)
	{
		OP_ASSERT(from.n_values == to.n_values);

		for (int i=0; i<from.n_values; i++)
		{
			if (from.value_type[i] != to.value_type[i])
			{
				if (from.value[i] == 0)
					from.value_type[i] = to.value_type[i];
				else if (to.value[i] == 0)
					to.value_type[i] = from.value_type[i];
				else
					return FALSE;
			}
		}
	}

	return TRUE;
}

void
CSS_transform_list::ResolveRelativeLengths(const CSSLengthResolver& length_resolver)
{
	CSS_transform_item* iter = static_cast<CSS_transform_item*>(values.First());
	while (iter)
	{
		iter->ResolveRelativeLengths(length_resolver);
		iter = iter->Suc();
	}
}

/* static */ BOOL
CSS_transform_list::MakeCompatible(CSS_transform_list& from, CSS_transform_list& to)
{
	CSS_transform_item* iter1 = static_cast<CSS_transform_item*>(from.values.First());
	CSS_transform_item* iter2 = static_cast<CSS_transform_item*>(to.values.First());

	OP_ASSERT(iter1 && iter2);

	while (iter1 && iter2)
	{
		if (!CSS_transform_item::MakeCompatible(*iter1, *iter2))
			break;

		iter1 = iter1->Suc();
		iter2 = iter2->Suc();
	}

	if (iter1 || iter2)
	{
		AffineTransform from_affine, to_affine;

		if (from.GetAffineTransform(from_affine) &&
			to.GetAffineTransform(to_affine))
		{
			CSS_transform_item* item = static_cast<CSS_transform_item*>(from.values.First());
			item->Out();
			from.values.Clear();
			item->Into(&from.values);
			item->SetFromAffineTransform(from_affine);

			item = static_cast<CSS_transform_item*>(to.values.First());
			item->Out();
			to.values.Clear();
			item->Into(&to.values);
			item->SetFromAffineTransform(to_affine);

			return TRUE;
		}
		else
			return FALSE;
	}
	else
		return TRUE;
}

static void InterpolateDecomposition(const AffineTransform::Decomposition& from,
									 const AffineTransform::Decomposition& to,
									 AffineTransform::Decomposition& target,
									 float percentage)
{
	target.tx = from.tx + (to.tx - from.tx) * percentage;
	target.ty = from.ty + (to.ty - from.ty) * percentage;

	target.sx = from.sx + (to.sx - from.sx) * percentage;
	target.sy = from.sy + (to.sy - from.sy) * percentage;

	target.rotate = from.rotate + (to.rotate - from.rotate) * percentage;
	target.shear = from.shear + (to.shear - from.shear) * percentage;
}

BOOL
CSS_transform_list::CSS_transform_item::Transition(const CSS_transform_list::CSS_transform_item& item1,
												   const CSS_transform_list::CSS_transform_item& item2,
												   float percentage)
{
	/* Make sure we're transitioning between compatible types. */
	OP_ASSERT(item1.type == item2.type &&
			  item1.n_values == item2.n_values &&
			  item1.n_values > 0);

	BOOL changed = FALSE;
	float new_val;

	switch (item1.type)
	{
	case CSS_VALUE_matrix:
		{
			OP_ASSERT(item1.n_values == 6);

			AffineTransform product, from_matrix, to_matrix;

			from_matrix.LoadValuesCM(item1.value[0], item1.value[1], item1.value[2],
									 item1.value[3], item1.value[4], item1.value[5]);
			to_matrix.LoadValuesCM(item2.value[0], item2.value[1], item2.value[2],
								   item2.value[3], item2.value[4], item2.value[5]);

			AffineTransform::Decomposition from_decomposed, to_decomposed;
			if (from_matrix.Decompose(from_decomposed) && to_matrix.Decompose(to_decomposed))
			{
				AffineTransform::Decomposition target;
				InterpolateDecomposition(from_decomposed, to_decomposed, target, percentage);

				product.Compose(target);
			}
			else
			{
				product.LoadIdentity();
			}

			/* 'item' is column-major, AffineTransform is row-major. */
			if (value[0] != product[0] ||
				value[1] != product[3] ||
				value[2] != product[1] ||
				value[3] != product[4] ||
				value[4] != product[2] ||
				value[5] != product[5])
			{
				value[0] = product[0];
				value[1] = product[3];
				value[2] = product[1];
				value[3] = product[4];
				value[4] = product[2];
				value[5] = product[5];
				changed = TRUE;
			}
		}
		break;

	default:
		for (int i=0; i<item1.n_values; i++)
		{
			new_val = item1.value[i] + (item2.value[i] - item1.value[i])*percentage;
			if (new_val != value[i])
			{
				value[i] = new_val;
				changed = TRUE;
			}
		}
		break;
	}

	return changed;
}

BOOL
CSS_transform_list::Transition(CSS_transform_list* from, CSS_transform_list* to, float percentage)
{
	const CSS_transform_item* iter_from = from->First();
	const CSS_transform_item* iter_to = to->First();
	CSS_transform_item* iter_this = static_cast<CSS_transform_item*>(values.First());

	BOOL changed = FALSE;
	while (iter_from && iter_to && iter_this)
	{
		if (iter_this->Transition(*iter_from, *iter_to, percentage))
			changed = TRUE;
		iter_from = iter_from->Suc();
		iter_to = iter_to->Suc();
		iter_this = iter_this->Suc();
	}

	/* from, to, and current should have the same number of items. */
	OP_ASSERT(!iter_from && !iter_to && !iter_this);

	return changed;
}

CSS_transform_list* CSS_transform_list::MakeCompatibleZeroList() const
{
	OpAutoPtr<CSS_transform_list> new_transform(OP_NEW(CSS_transform_list, (CSS_PROPERTY_transform)));
	if (!new_transform.get())
		return NULL;

	const CSS_transform_item* item = First();
	while (item)
	{
		CSS_transform_item* new_item = OP_NEW(CSS_transform_item, ());
		if (!new_item)
			return NULL;
		new_item->MakeCompatibleZero(item);
		new_item->Into(&new_transform->values);
		item = item->Suc();
	}
	return new_transform.release();
}

# endif // CSS_TRANSITIONS

#endif // CSS_TRANSFORMS

#ifdef _DEBUG

Debug&
operator<<(Debug& d, const CSS_decl& decl)
{
	TempBuffer buf;
	CSS::FormatDeclarationL(&buf, &decl, FALSE, CSS_FORMAT_COMPUTED_STYLE);
	d << buf.GetStorage();
	return d;
}

#endif // _DEBUG
