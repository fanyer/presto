/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#include "core/pch.h"

#if defined(SVG_SUPPORT) && defined(SVG_DOM) && defined(SVG_FULL_11)

#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/svgdom/svgdomanimatedvalueimpl.h"

#include "modules/svg/src/svgdom/svgdommatriximpl.h"
#include "modules/svg/src/svgdom/svgdomstringimpl.h"
#include "modules/svg/src/svgdom/svgdomstringurlimpl.h"
#include "modules/svg/src/svgdom/svgdomnumberimpl.h"
#include "modules/svg/src/svgdom/svgdomnumberorpercentageimpl.h"
#include "modules/svg/src/svgdom/svgdompreserveaspectratioimpl.h"
#include "modules/svg/src/svgdom/svgdomenumerationimpl.h"
#include "modules/svg/src/svgdom/svgdomlengthimpl.h"
#include "modules/svg/src/svgdom/svgdomangleimpl.h"
#include "modules/svg/src/svgdom/svgdomrectimpl.h"
#include "modules/svg/src/svgdom/svgdompointimpl.h"
#include "modules/svg/src/svgdom/svgdomtransformimpl.h"
#include "modules/svg/src/svgdom/svgdompaintimpl.h"
#include "modules/svg/src/svgdom/svgdomlistimpl.h"

#include "modules/svg/src/svgdom/svgdomanimatedtransformlistimpl.h"

#include "modules/logdoc/logdoc.h"

SVGDOMAnimatedValueImpl::SVGDOMAnimatedValueImpl(SVGObject* b, SVGObject* a,
												 const char* n,
												 SVGDOMItemType itemtype,
												 SVGDOMItemType sub_itemtype) :
	m_itemtype(itemtype),
	m_sub_itemtype(sub_itemtype),
	m_base(b),
	m_anim(a),
	m_name(n)
{
	SVGObject::IncRef(m_base);
	SVGObject::IncRef(m_anim);
}

/* static */ OP_STATUS
SVGDOMAnimatedValueImpl::Make(SVGDOMAnimatedValueImpl*& animated_value,
							  SVGObject* base_obj, SVGObject* anim_obj,
							  SVGDOMItemType itemtype, SVGDOMItemType sub_itemtype /* = SVG_DOM_ITEMTYPE_UNKNOWN */)
{
	const char* name = LowGetDOMName(itemtype, sub_itemtype);
	if (!(animated_value = OP_NEW(SVGDOMAnimatedValueImpl, (base_obj, anim_obj, name, itemtype, sub_itemtype))))
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}

/* virtual */
SVGDOMAnimatedValueImpl::~SVGDOMAnimatedValueImpl()
{
	SVGObject::DecRef(m_base);
	SVGObject::DecRef(m_anim);
}

/* virtual */ OP_BOOLEAN
SVGDOMAnimatedValueImpl::GetPrimitiveValue(SVGDOMPrimitiveValue &value, Field field)
{
	SVGObject *obj = (field == FIELD_ANIM) ? m_anim : m_base;
	return LowGetValue(value, m_itemtype, m_sub_itemtype, obj);
}

/* virtual */ const char*
SVGDOMAnimatedValueImpl::GetDOMName()
{
	return m_name;
}

/* static */ const char*
SVGDOMAnimatedValueImpl::LowGetDOMName(SVGDOMItemType type, SVGDOMItemType subtype)
{
	switch(type)
	{
		case SVG_DOM_ITEMTYPE_STRING:
			return "SVGAnimatedString";
		case SVG_DOM_ITEMTYPE_ENUM:
			return "SVGAnimatedEnumeration";
		case SVG_DOM_ITEMTYPE_BOOLEAN:
			return "SVGAnimatedBoolean";
		case SVG_DOM_ITEMTYPE_NUMBER:
			return "SVGAnimatedNumber";
		case SVG_DOM_ITEMTYPE_LENGTH:
			return "SVGAnimatedLength";
		case SVG_DOM_ITEMTYPE_PRESERVE_ASPECT_RATIO:
			return "SVGAnimatedPreserveAspectRatio";
		case SVG_DOM_ITEMTYPE_RECT:
			return "SVGAnimatedRect";
		case SVG_DOM_ITEMTYPE_ANGLE:
			return "SVGAnimatedAngle";
		case SVG_DOM_ITEMTYPE_NUMBER_OPTIONAL_NUMBER_0:
		case SVG_DOM_ITEMTYPE_NUMBER_OPTIONAL_NUMBER_1:
			return "SVGAnimatedNumber";
		case SVG_DOM_ITEMTYPE_INTEGER_OPTIONAL_INTEGER_0:
		case SVG_DOM_ITEMTYPE_INTEGER_OPTIONAL_INTEGER_1:
			return "SVGAnimatedInteger";
		case SVG_DOM_ITEMTYPE_LIST:
			switch(subtype)
			{
				case SVG_DOM_ITEMTYPE_POINT:
					return "SVGAnimatedPointList";
				case SVG_DOM_ITEMTYPE_NUMBER:
					return "SVGAnimatedNumberList";
				case SVG_DOM_ITEMTYPE_LENGTH:
					return "SVGAnimatedLengthList";
				case SVG_DOM_ITEMTYPE_MATRIX:
					return "SVGAnimatedMatrixList";
				default:
					OP_ASSERT(!"Unknown animated list type");
					return "SVGAnimatedList";
			}
		default:
			// Silence compiler (and catch future errors)
			OP_ASSERT(!"Not reached");
			return "SVGAnimatedValue";
	}
}

/* virtual */ BOOL
SVGDOMAnimatedValueImpl::IsSetByNumber()
{
	return m_itemtype == SVG_DOM_ITEMTYPE_ENUM ||
		m_itemtype == SVG_DOM_ITEMTYPE_BOOLEAN ||
		m_itemtype == SVG_DOM_ITEMTYPE_NUMBER ||
		m_itemtype == SVG_DOM_ITEMTYPE_NUMBER_OPTIONAL_NUMBER_0 ||
		m_itemtype == SVG_DOM_ITEMTYPE_NUMBER_OPTIONAL_NUMBER_1 ||
		m_itemtype == SVG_DOM_ITEMTYPE_INTEGER_OPTIONAL_INTEGER_0 ||
		m_itemtype == SVG_DOM_ITEMTYPE_INTEGER_OPTIONAL_INTEGER_1;
}

/* virtual */ OP_BOOLEAN
SVGDOMAnimatedValueImpl::SetNumber(double number)
{
	if (m_itemtype == SVG_DOM_ITEMTYPE_ENUM)
	{
		OP_ASSERT(m_base->Type() == SVGOBJECT_ENUM);
		SVGEnum *enum_obj = static_cast<SVGEnum *>(m_base);

		RETURN_FALSE_IF(enum_obj->EnumValue() == (unsigned short)number);
		enum_obj->SetEnumValue((unsigned short)number);
		return OpBoolean::IS_TRUE;
	}
	else if (m_itemtype == SVG_DOM_ITEMTYPE_BOOLEAN)
	{
		SVGEnum *enum_obj = static_cast<SVGEnum *>(m_base);
		BOOL value = (number != 0.0);

		RETURN_FALSE_IF(!!enum_obj->EnumValue() == value);
		enum_obj->SetEnumValue(value);
		return OpBoolean::IS_TRUE;
	}
	else if (m_itemtype == SVG_DOM_ITEMTYPE_NUMBER)
	{
		SVGNumberObject *number_obj = static_cast<SVGNumberObject *>(m_base);
		RETURN_FALSE_IF(number_obj->value == number); // Should we do a epsilon comparison here?
		number_obj->value = number;
		return OpBoolean::IS_TRUE;
	}
	else if (m_itemtype == SVG_DOM_ITEMTYPE_NUMBER_OPTIONAL_NUMBER_0 ||
			 m_itemtype == SVG_DOM_ITEMTYPE_NUMBER_OPTIONAL_NUMBER_1 ||
			 m_itemtype == SVG_DOM_ITEMTYPE_INTEGER_OPTIONAL_INTEGER_0 ||
			 m_itemtype == SVG_DOM_ITEMTYPE_INTEGER_OPTIONAL_INTEGER_1)
	{
		OP_ASSERT (m_base->Type() == SVGOBJECT_VECTOR);
		SVGVector *vector = static_cast<SVGVector *>(m_base);
		OP_ASSERT (vector->VectorType() == SVGOBJECT_NUMBER);

		int idx = 0;
		if (m_itemtype == SVG_DOM_ITEMTYPE_NUMBER_OPTIONAL_NUMBER_1 ||
			m_itemtype == SVG_DOM_ITEMTYPE_INTEGER_OPTIONAL_INTEGER_1)
		{
			idx = 1;
		}

		if (idx == 0 && vector->GetCount() > 0)
		{
			static_cast<SVGNumberObject *>(vector->Get(0))->value = number;
		}
		else if (idx == 1 && vector->GetCount() > 1)
		{
			static_cast<SVGNumberObject *>(vector->Get(1))->value = number;
		}
		else
		{
			// We just add it. In the case we are adding the second
			// parameter, add two so that the first parameter gets the
			// same value as the second (that we are trying to set).
			if (idx == 1 && vector->GetCount() == 0)
			{
				if (OpStatus::IsMemoryError(vector->Append(OP_NEW(SVGNumberObject, (number)))) ||
					OpStatus::IsMemoryError(vector->Append(OP_NEW(SVGNumberObject, (number)))))
				{
					return OpStatus::ERR_NO_MEMORY;
				}
			}
			else
			{
				RETURN_IF_MEMORY_ERROR(vector->Append(OP_NEW(SVGNumberObject, (number))));
			}
		}
	}

	return OpBoolean::IS_FALSE;
}

/* virtual */ BOOL
SVGDOMAnimatedValueImpl::IsSetByString()
{
	return m_itemtype == SVG_DOM_ITEMTYPE_STRING;
}

/* virtual */ OP_BOOLEAN
SVGDOMAnimatedValueImpl::SetString(const uni_char *str, LogicalDocument* logdoc)
{
	if (m_itemtype == SVG_DOM_ITEMTYPE_STRING)
	{
		if (m_base->Type() == SVGOBJECT_STRING)
		{
			unsigned str_length = uni_strlen(str);
			SVGString *str_obj = static_cast<SVGString *>(m_base);

			RETURN_FALSE_IF(str_length == str_obj->GetLength() && str_obj->GetString() &&
							uni_strcmp(str_obj->GetString(), str) == 0);

			RETURN_IF_ERROR(str_obj->SetString(str, str_length));
			return OpBoolean::IS_TRUE;
		}
		else if (m_base->Type() == SVGOBJECT_URL)
		{
			unsigned str_length = uni_strlen(str);
			SVGURL *url_obj = static_cast<SVGURL *>(m_base);

			RETURN_FALSE_IF(str_length == url_obj->GetAttrStringLength() &&
							uni_strcmp(url_obj->GetAttrString(), str) == 0);

			RETURN_IF_ERROR(static_cast<SVGURL *>(m_base)->SetURL(str, URL()));
			return OpBoolean::IS_TRUE;
		}
		else if (m_base->Type() == SVGOBJECT_CLASSOBJECT)
		{
			if (logdoc)
			{
				SVGClassObject *class_obj = static_cast<SVGClassObject *>(m_base);
				ClassAttribute* class_attr = logdoc->CreateClassAttribute(str, uni_strlen(str));
				if (class_attr)
				{
					class_obj->SetClassAttribute(class_attr);
					return OpBoolean::IS_TRUE;
				}
				else
					return OpStatus::ERR_NO_MEMORY;
			}
		}
	}
	return OpBoolean::IS_FALSE;
}

/* static */ OP_BOOLEAN
SVGDOMAnimatedValueImpl::LowGetValue(SVGDOMPrimitiveValue &value,
									 SVGDOMItemType type, SVGDOMItemType subtype, SVGObject *source)
{
	if (!source)
	{
		return OpBoolean::IS_FALSE;
	}

	switch(type)
	{
#ifdef SVG_FULL_11
		case SVG_DOM_ITEMTYPE_ANGLE:
		{
			OP_ASSERT(source->Type() == SVGOBJECT_ANGLE);
			value.type = SVGDOMPrimitiveValue::VALUE_ITEM;
			value.value.item = OP_NEW(SVGDOMAngleImpl, (static_cast<SVGAngle*>(source)));
		}
		break;
#endif // SVG_FULL_11
		case SVG_DOM_ITEMTYPE_RECT:
		{
			OP_ASSERT(source->Type() == SVGOBJECT_RECT);
			value = OP_NEW(SVGDOMRectImpl, (static_cast<SVGRectObject*>(source)));
			value.type = SVGDOMPrimitiveValue::VALUE_ITEM;
		}
		break;
		case SVG_DOM_ITEMTYPE_PRESERVE_ASPECT_RATIO:
		{
			OP_ASSERT(source->Type() == SVGOBJECT_ASPECT_RATIO);
			value = OP_NEW(SVGDOMPreserveAspectRatioImpl, (static_cast<SVGAspectRatio*>(source)));
			value.type = SVGDOMPrimitiveValue::VALUE_ITEM;
		}
		break;
		case SVG_DOM_ITEMTYPE_LENGTH:
		{
			OP_ASSERT(source->Type() == SVGOBJECT_LENGTH);
			value = OP_NEW(SVGDOMLengthImpl, (static_cast<SVGLengthObject*>(source)));
			value.type = SVGDOMPrimitiveValue::VALUE_ITEM;
		}
		break;
		case SVG_DOM_ITEMTYPE_NUMBER:
		{
			if (source->Type() == SVGOBJECT_NUMBER)
			{
				value.value.number = static_cast<SVGNumberObject*>(source)->value.GetFloatValue();
				value.type = SVGDOMPrimitiveValue::VALUE_NUMBER;
			}
			else if (source->Type() == SVGOBJECT_LENGTH)
			{
				value.value.item = OP_NEW(SVGDOMLengthImpl, (static_cast<SVGLengthObject*>(source)));
				value.type = SVGDOMPrimitiveValue::VALUE_ITEM;
			}
		}
		break;
		case SVG_DOM_ITEMTYPE_NUMBER_OPTIONAL_NUMBER_0:
		case SVG_DOM_ITEMTYPE_NUMBER_OPTIONAL_NUMBER_1:
		case SVG_DOM_ITEMTYPE_INTEGER_OPTIONAL_INTEGER_0:
		case SVG_DOM_ITEMTYPE_INTEGER_OPTIONAL_INTEGER_1:
		{
			int idx = 0;
			if (type == SVG_DOM_ITEMTYPE_NUMBER_OPTIONAL_NUMBER_1 ||
				type == SVG_DOM_ITEMTYPE_INTEGER_OPTIONAL_INTEGER_1)
			{
				idx = 1;
			}

			OP_ASSERT (source->Type() == SVGOBJECT_VECTOR);

			SVGVector *vector = static_cast<SVGVector*>(source);
			SVGObject *obj = NULL;

			if (idx == 1 && vector->GetCount() > 1)
				obj = vector->Get(1);
			else if (vector->GetCount() > 0)
				obj = vector->Get(0);

			if (obj)
				value.value.number = static_cast<SVGNumberObject*>(obj)->value.GetFloatValue();
			else
				value.value.number = 0.0; // Default value ??

			value.type = SVGDOMPrimitiveValue::VALUE_NUMBER;
		}
		break;
		case SVG_DOM_ITEMTYPE_ENUM:
		{
			OP_ASSERT(source->Type() == SVGOBJECT_ENUM);
			value.value.number = static_cast<SVGEnum*>(source)->EnumValue();
			value.type = SVGDOMPrimitiveValue::VALUE_NUMBER;
		}
		break;
		case SVG_DOM_ITEMTYPE_BOOLEAN:
		{
			OP_ASSERT(source->Type() == SVGOBJECT_ENUM);
			value.value.boolean = !!static_cast<SVGEnum*>(source)->EnumValue();
			value.type = SVGDOMPrimitiveValue::VALUE_BOOLEAN;
		}
		break;
		case SVG_DOM_ITEMTYPE_STRING:
		{
			value.type = SVGDOMPrimitiveValue::VALUE_STRING;

			if (source->Type() == SVGOBJECT_STRING)
				value.value.str = static_cast<SVGString *>(source)->GetString();
			else if (source->Type() == SVGOBJECT_URL)
				value.value.str = static_cast<SVGURL *>(source)->GetAttrString();
			else if (source->Type() == SVGOBJECT_CLASSOBJECT)
			{
				const ClassAttribute* class_attr = static_cast<SVGClassObject *>(source)->GetClassAttribute();
				value.value.str = class_attr ? class_attr->GetAttributeString() : NULL;
			}
			else
			{
				OP_ASSERT(!"Unhandled object type");
				value.type = SVGDOMPrimitiveValue::VALUE_NONE;
			}
		}
		break;
		case SVG_DOM_ITEMTYPE_LIST:
		{
			OP_ASSERT(source->Type() == SVGOBJECT_VECTOR);

			value.value.item = OP_NEW(SVGDOMListImpl, (subtype, static_cast<SVGVector*>(source)));
			value.type = SVGDOMPrimitiveValue::VALUE_ITEM;
		}
		break;
		default:
		{
			OP_ASSERT(!"Not reached");
			return OpBoolean::IS_FALSE;
		}
	}

	if (value.type == SVGDOMPrimitiveValue::VALUE_ITEM && !value.value.item)
		return OpStatus::ERR_NO_MEMORY;

	return OpBoolean::IS_TRUE;
}

SVGDOMAnimatedValueTransformListImpl::SVGDOMAnimatedValueTransformListImpl(
	SVGVector* base_vector, SVGTransform* anim_transform, BOOL additive) :
	m_base(base_vector),
	m_anim(anim_transform),
	m_additive(additive)
{
	SVGObject::IncRef(m_base);
	SVGObject::IncRef(m_anim);
}

/* virtual */
SVGDOMAnimatedValueTransformListImpl::~SVGDOMAnimatedValueTransformListImpl()
{
	SVGObject::DecRef(m_base);
	SVGObject::DecRef(m_anim);
}

/* virtual */ OP_BOOLEAN
SVGDOMAnimatedValueTransformListImpl::GetPrimitiveValue(SVGDOMPrimitiveValue &value, Field field)
{
	if (field == FIELD_ANIM)
	{
		SVGDOMAnimatedTransformListImpl *list_value;
		RETURN_IF_ERROR(SVGDOMAnimatedTransformListImpl::Make(list_value, m_additive ? m_base : NULL, m_anim));
		value.value.item = list_value;
	}
	else
	{
		value.value.item = OP_NEW(SVGDOMListImpl, (SVG_DOM_ITEMTYPE_TRANSFORM, m_base));
		if (!value.value.item)
			return OpStatus::ERR_NO_MEMORY;
	}

	value.type = SVGDOMPrimitiveValue::VALUE_ITEM;
	return value.value.item ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

/* virtual */ const char *
SVGDOMAnimatedValueTransformListImpl::GetDOMName()
{
	return "SVGAnimatedTransformList";
}

#endif // SVG_SUPPORT && SVG_DOM && SVG_FULL_11
