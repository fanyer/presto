/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2010-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/SVGAttribute.h"
#include "modules/svg/src/SVGUtils.h"

#include "modules/encodings/decoders/utf8-decoder.h"
#include "modules/encodings/encoders/utf8-encoder.h"

SVGAttribute::SVGAttribute(SVGObject* base) :
		ComplexAttr(),
		base(base),
		string_rep(NULL),
		anim_data(NULL)
{
	serial = SVGUtils::GetNextSerial();
	SVGObject::IncRef(base);
}

/* virtual */
SVGAttribute::~SVGAttribute()
{
	SVGObject::DecRef(base);
	OP_DELETE(anim_data);
	OP_DELETEA(string_rep);
}

void
SVGAttribute::ReplaceBaseObject(SVGObject* obj)
{
	SVGObject* old_obj = base;
	base = obj;
	SVGObject::IncRef(obj);
	SVGObject::DecRef(old_obj);
}

OP_STATUS
SVGAttribute::SetAnimationObject(SVGAttributeField field, SVGObject* obj)
{
	OP_ASSERT(field == SVG_ATTRFIELD_ANIM || field == SVG_ATTRFIELD_CSS);

	RETURN_IF_ERROR(AssertAnimationData());

	SVGObject* old_obj = NULL;

	if (field == SVG_ATTRFIELD_ANIM)
	{
		old_obj = anim_data->anim;
		anim_data->anim  = obj;
	}
	else
	{
		old_obj = anim_data->css;
		anim_data->css  = obj;
	}

	serial = SVGUtils::GetNextSerial();
	SVGObject::IncRef(obj);
	SVGObject::DecRef(old_obj);
	return OpStatus::OK;
}

BOOL
SVGAttribute::ActivateAnimationObject(SVGAttributeField field)
{
	BOOL was_active = anim_data && (anim_data->info.is_anim_active || anim_data->info.is_css_active);

	if (anim_data)
	{
		if (field == SVG_ATTRFIELD_ANIM)
		{
			anim_data->info.is_anim_active = TRUE;
		}
		else if (field == SVG_ATTRFIELD_CSS)
		{
			anim_data->info.is_css_active = TRUE;
		}
	}

	return was_active;
}

BOOL
SVGAttribute::DeactivateAnimation()
{
	BOOL was_active = FALSE;

	if (anim_data)
	{
		was_active = anim_data->info.is_anim_active || anim_data->info.is_css_active;

		anim_data->info.is_anim_active = FALSE;
		anim_data->info.is_css_active = FALSE;
	}

	return was_active;
}

/* virtual */ OP_STATUS
SVGAttribute::CreateCopy(ComplexAttr **copy_to)
{
	OpAutoPtr<SVGObject> nbase(NULL);

	if(base)
	{
		SVGObject* obj = base->Clone();
		if (!obj)
			return OpStatus::ERR_NO_MEMORY;
		nbase.reset(obj);
	}

	OpAutoPtr<SVGAttribute> attr_copy(OP_NEW(SVGAttribute, (nbase.get())));
	if (!attr_copy.get())
		return OpStatus::ERR_NO_MEMORY;

	nbase.release();

	if(anim_data)
	{
		RETURN_IF_ERROR(attr_copy->AssertAnimationData());
		if (anim_data->anim)
		{
			attr_copy->anim_data->anim = anim_data->anim->Clone();
			if (!attr_copy->anim_data->anim)
				return OpStatus::ERR_NO_MEMORY;
			SVGObject::IncRef(attr_copy->anim_data->anim);
		}
		if(anim_data->css)
		{
			attr_copy->anim_data->css = anim_data->css->Clone();
			if (!attr_copy->anim_data->css)
				return OpStatus::ERR_NO_MEMORY;
			SVGObject::IncRef(attr_copy->anim_data->css);
		}
	}

	if(string_rep)
	{
		int len = op_strlen(string_rep);
		attr_copy->string_rep = OP_NEWA(char, op_strlen(string_rep) + 1);
		if(!attr_copy->string_rep)
			return OpStatus::ERR_NO_MEMORY;

		op_strcpy(attr_copy->string_rep, string_rep);
		attr_copy->string_rep[len] = 0;
	}

	*copy_to = attr_copy.release();
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
SVGAttribute::ToString(TempBuffer *buffer)
{
	if(!buffer)
		return OpStatus::ERR_NULL_POINTER;

	buffer->Clear(); // Wipe buffer

	if (string_rep != NULL)
	{
		return SetTempBufferFromUTF8(buffer, string_rep);
	}
	else if (base != NULL)
	{
		return base->GetStringRepresentation(buffer);
	}
	else
	{
		return OpStatus::ERR;
	}
}

/* virtual */ BOOL
SVGAttribute::MoveToOtherDocument(FramesDocument *old_document, FramesDocument *new_document)
{
	/* For class attributes, we need to get new class references for the ClassAttribute objects
	   because the class references are unique per LogicalDocument. If any problems occurred
	   moving the ClassAttribute objects, don't move the SVGAttribute, because the state will
	   be inconsistent.

	   For all other attributes, moving the attribute unchanged is unproblematic, so we can
	   return TRUE. */

	if (base && base->Type() == SVGOBJECT_CLASSOBJECT)
	{
		OP_DELETE(anim_data);
		anim_data = NULL;

		SVGClassObject* class_obj = static_cast<SVGClassObject*>(base);
		ClassAttribute* class_attr = class_obj->GetClassAttribute();
		if (class_attr && !class_attr->MoveToOtherDocument(old_document, new_document))
			return FALSE;
	}

	return TRUE;
}

/* virtual */ BOOL
SVGAttribute::Equals(ComplexAttr *other)
{
	OP_ASSERT(other);

	if (!other->IsA(SVG_T_SVGATTR))
		return FALSE;

	SVGAttribute* other_svg = static_cast<SVGAttribute*>(other);

	SVGObject* obj1 = GetSVGObject(SVG_ATTRFIELD_BASE);
	SVGObject* obj2 = other_svg->GetSVGObject(SVG_ATTRFIELD_BASE);
	if (!obj1 || !obj2 || obj1->Type() != obj2->Type())
		return FALSE;

	return obj1->IsEqual(*obj2);
}

OP_STATUS
SVGAttribute::SetOverrideString(const uni_char *override_string, unsigned override_string_length)
{
	OP_ASSERT(string_rep == NULL); // This function must only be
								   // called once per instance

	UTF16toUTF8Converter decoder;
	int bytes_needed = decoder.BytesNeeded((const char *) override_string,
										   UNICODE_SIZE(override_string_length));
	decoder.Reset();

	string_rep = OP_NEWA(char, bytes_needed + 1);
	if (!string_rep)
		return OpStatus::ERR_NO_MEMORY;

	int read_bytes;
	int written = decoder.Convert(override_string, UNICODE_SIZE(override_string_length),
								  string_rep, bytes_needed, &read_bytes);
	string_rep[written] = 0;
	return OpStatus::OK;
}

void
SVGAttribute::ClearOverrideString()
{
	OP_DELETEA(string_rep);
	string_rep = NULL;
}

SVGObject*
SVGAttribute::GetSVGObject(SVGAttributeField field_type /* = SVG_ATTRFIELD_DEFAULT */, 
					  SVGAttributeType anim_attribute_type /* = SVGATTRIBUTE_AUTO */)
{
	if(anim_attribute_type == SVGATTRIBUTE_AUTO || anim_attribute_type == SVGATTRIBUTE_XML)
	{
		if (field_type == SVG_ATTRFIELD_DEFAULT)
		{
			if (anim_data)
			{
				if(anim_data->info.is_css_active && anim_attribute_type == SVGATTRIBUTE_AUTO)
					return anim_data->css;
				else if (anim_data->info.is_anim_active)
					return anim_data->anim;
				else
					return base;
			}
			else
			{
				return base;
			}
		}
		else if (field_type == SVG_ATTRFIELD_ANIM && anim_data && anim_data->info.is_anim_active)
		{
			return anim_data->anim;
		}
		else if (field_type == SVG_ATTRFIELD_BASE)
		{
			return base;
		}
	}
	else if(anim_attribute_type == SVGATTRIBUTE_CSS && anim_data && anim_data->info.is_css_active)
	{
		return anim_data->css;
	}

	return NULL;
}

OP_STATUS
SVGAttribute::AssertAnimationData()
{
	if (!anim_data)
	{
		anim_data = OP_NEW(AnimationData, ());
		if (!anim_data)
			return OpStatus::ERR_NO_MEMORY;
	}

	OP_ASSERT(anim_data != NULL);

	return OpStatus::OK;
}

SVGAttribute::AnimationData::AnimationData() :
		anim(NULL),
		css(NULL)
{
	info_packed = 0x0;
}

SVGAttribute::AnimationData::~AnimationData()
{
	SVGObject::DecRef(anim);
	SVGObject::DecRef(css);
}

SVGObject *
SVGAttribute::GetAnimationSVGObject(SVGAttributeType anim_attribute_type)
{
	if (anim_data != NULL)
	{
		if (anim_attribute_type == SVGATTRIBUTE_AUTO)
		{
			if (anim_data->css)
				return anim_data->css;
			else
				return anim_data->anim;
		}
		else if (anim_attribute_type == SVGATTRIBUTE_CSS)
			return anim_data->css;
		else
			return anim_data->anim;
	}

	return NULL;
}

void
SVGAttribute::MarkAsInitialized()
{
	// Only base values can be UNINITIALIZED
	if (base != NULL)
	{
		base->UnsetFlag(SVGOBJECTFLAG_UNINITIALIZED);
	}
}

/* static */
OP_STATUS
SVGAttribute::SetTempBufferFromUTF8(TempBuffer* buffer, const char* utf_8_str)
{
	if (utf_8_str)
	{
		unsigned length_in_bytes = op_strlen(utf_8_str);

		UTF8toUTF16Converter decoder;
		int bytes_needed = decoder.Convert(utf_8_str, length_in_bytes, NULL, INT_MAX, NULL);
		// size is already number of bytes, but does not include trailing null
		unsigned char_space_needed = buffer->Length() + UNICODE_DOWNSIZE(bytes_needed) + 1;
		RETURN_IF_ERROR(buffer->Expand(char_space_needed));
		uni_char* buf_p = buffer->GetStorage() + buffer->Length();
		decoder.Convert(utf_8_str, length_in_bytes, (char*)buf_p, bytes_needed, NULL);
		buf_p[UNICODE_DOWNSIZE(bytes_needed)] = '\0';
		buffer->SetCachedLengthPolicy(TempBuffer::UNTRUSTED); // In case someone has set it to trusted
	}
	return OpStatus::OK;
}

BOOL
SVGAttributeIterator::GetNext(int& attr_name, int& ns_idx, BOOL& is_special, SVGObject*& obj)
{
	ItemType item_type;
	void* pobj;

	obj = NULL;

	while (HTML_ImmutableAttrIterator::GetNext(attr_name, ns_idx, is_special, pobj, item_type))
	{
		if (item_type == ITEM_TYPE_COMPLEX &&
			pobj && static_cast<ComplexAttr*>(pobj)->IsA(SVG_T_SVGATTR))
		{
			ComplexAttr* complex_attr = static_cast<ComplexAttr *>(pobj);
			OP_ASSERT(complex_attr->IsA(SVG_T_SVGATTR));

			SVGAttribute* attr = static_cast<SVGAttribute *>(complex_attr);
			obj = attr->GetSVGObject(m_field_type, m_anim_attr_type);

			return TRUE;
		}
	}

	return FALSE;
}

#endif // SVG_SUPPORT
