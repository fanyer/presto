/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/AttrValueStore.h"

#include "modules/logdoc/htm_elm.h"
#include "modules/logdoc/namespace.h"

#include "modules/svg/src/SVGInternalEnum.h"
#include "modules/svg/src/SVGValue.h"
#include "modules/svg/src/SVGAnimationContext.h"
#include "modules/svg/src/SVGElementStateContext.h"
#include "modules/svg/src/SVGDocumentContext.h"
#include "modules/svg/src/SVGUtils.h"
#include "modules/svg/src/SVGFontSize.h"
#include "modules/svg/src/SVGTransform.h"
#include "modules/svg/src/OpBpath.h"

void SVGTrfmCalcHelper::GetMatrix(SVGMatrix& matrix) const
{
	matrix.LoadIdentity();

	if (HasTransform(ANIM_TRANSFORM_MATRIX))
		matrix.Multiply(m_transform[ANIM_TRANSFORM_MATRIX]);

	if (HasTransform(MOTION_TRANSFORM_MATRIX))
		matrix.Multiply(m_transform[MOTION_TRANSFORM_MATRIX]);

	if (!HasTransform(REF_TRANSFORM_MATRIX) && HasTransform(BASE_TRANSFORM_MATRIX))
		matrix.Multiply(m_transform[BASE_TRANSFORM_MATRIX]);
}

static BOOL IsTransformAllowed(HTML_Element* e)
{
	OP_ASSERT(e && e->GetNsType() == NS_SVG);

	/* This list of elements if from the SVG 1.1 specification,
	 * attribute index, transform attribute.
	 */
	Markup::Type type = e->Type();
	if (type == Markup::SVGE_G ||
		type == Markup::SVGE_DEFS ||
		type == Markup::SVGE_USE ||
		type == Markup::SVGE_IMAGE ||
		type == Markup::SVGE_SWITCH ||
		type == Markup::SVGE_PATH ||
		type == Markup::SVGE_RECT ||
		type == Markup::SVGE_CIRCLE ||
		type == Markup::SVGE_ELLIPSE ||
		type == Markup::SVGE_LINE ||
		type == Markup::SVGE_POLYLINE ||
		type == Markup::SVGE_POLYGON ||
		type == Markup::SVGE_TEXT ||
		type == Markup::SVGE_TEXTAREA ||
		type == Markup::SVGE_CLIPPATH ||
		type == Markup::SVGE_A ||
		type == Markup::SVGE_FOREIGNOBJECT ||
		type == Markup::SVGE_VIDEO)
		return TRUE;

	return FALSE;
}

// This function calculates matrices from transform lists and puts
// them in the helper object that then can be used for getting
// the current transform where it is needed.
//
// There are special logic here for additive transformation animations
// if SVG_APPLY_ANIMATE_MOTION_IN_BETWEEN is set.  It is controlled by
// the Markup::SVGA_ANIMATE_TRANSFORM_ADDITIVE attribute.
void SVGTrfmCalcHelper::Setup(HTML_Element* e)
{
	SVGMatrix matrix;

	if (AttrValueStore::HasObject(e, Markup::SVGA_MOTION_TRANSFORM, SpecialNs::NS_SVG, TRUE))
	{
		AttrValueStore::GetMatrix(e, Markup::SVGA_MOTION_TRANSFORM, matrix);
		SetTransformMatrix(MOTION_TRANSFORM_MATRIX, matrix);
	}

#ifdef SVG_APPLY_ANIMATE_MOTION_IN_BETWEEN
	if (AttrValueStore::HasObject(e, Markup::SVGA_ANIMATE_TRANSFORM, SpecialNs::NS_SVG, TRUE))
	{
		AttrValueStore::GetMatrix(e, Markup::SVGA_ANIMATE_TRANSFORM, matrix);
		SetTransformMatrix(ANIM_TRANSFORM_MATRIX, matrix);
	}
#endif // SVG_APPLY_ANIMATE_MOTION_IN_BETWEEN

	SVGMatrix ref_translation;
	if (AttrValueStore::HasRefTransform(e, Markup::SVGA_TRANSFORM, ref_translation))
	{
		SetRefTransform(ref_translation);
	}
	else
	{
#ifdef SVG_APPLY_ANIMATE_MOTION_IN_BETWEEN
		BOOL additive = ATTR_VAL_AS_NUM(e->GetSpecialAttr(Markup::SVGA_ANIMATE_TRANSFORM_ADDITIVE, ITEM_TYPE_NUM,
														  NUM_AS_ATTR_VAL(TRUE), SpecialNs::NS_SVG)) != FALSE;
#else
		BOOL additive = TRUE;
#endif

		if (additive && IsTransformAllowed(e) && AttrValueStore::HasObject(e, Markup::SVGA_TRANSFORM, NS_IDX_SVG))
		{
			AttrValueStore::GetMatrix(e, Markup::SVGA_TRANSFORM, matrix);
			SetTransformMatrix(BASE_TRANSFORM_MATRIX, matrix);
		}
	}
}

SVGAttribute* AttrValueStore::GetSVGAttr(HTML_Element* elm, Markup::AttrType attr_name, int ns_idx, BOOL is_special)
{
#ifdef _DEBUG
	if (!is_special)
	{
		NS_Type attr_ns = g_ns_manager->GetNsTypeAt(elm->ResolveNsIdx(ns_idx));
		OP_ASSERT(attr_ns != NS_SVG ||
			((!is_special && !Markup::IsSpecialAttribute(attr_name)) || (Markup::IsSpecialAttribute(attr_name) && is_special))); // check "hidden" svg attributes
	}
#endif // _DEBUG

	ComplexAttr* attr;
	if (is_special)
		attr = static_cast<ComplexAttr*>(elm->GetSpecialAttr(attr_name, ITEM_TYPE_COMPLEX, NULL, static_cast<SpecialNs::Ns>(ns_idx)));
	else
		attr = static_cast<ComplexAttr*>(elm->GetAttr(attr_name, ITEM_TYPE_COMPLEX, NULL, ns_idx));
	if (!attr)
		return NULL;

	return attr->IsA(SVG_T_SVGATTR) ? static_cast<SVGAttribute*>(attr) : NULL;
}

BOOL
AttrValueStore::HasObject(HTML_Element* e, Markup::AttrType attr_name, int ns_idx, BOOL is_special,
						  BOOL base /* = FALSE */, SVGAttributeType anim_attribute_type /* = SVGATTRIBUTE_AUTO */)
{
	SVGAttribute* svg_attr = GetSVGAttr(e, attr_name, ns_idx, is_special);
	return (svg_attr != NULL) && svg_attr->HasSVGObject(base ? SVG_ATTRFIELD_BASE : SVG_ATTRFIELD_DEFAULT, anim_attribute_type);
}

BOOL
AttrValueStore::HasTransform(HTML_Element* e, Markup::AttrType attr_name, int ns_idx)
{
	return HasObject(e, attr_name, ns_idx) || HasObject(e, Markup::SVGA_ANIMATE_TRANSFORM, SpecialNs::NS_SVG, TRUE);
}

/* static */ OP_STATUS
AttrValueStore::GetObject(HTML_Element *e, Markup::AttrType attr_type, int ns_idx,
						  BOOL is_special,
						  SVGObjectType object_type, SVGObject** obj,
						  SVGAttributeField field_type,
						  SVGAttributeType anim_attribute_type /* = SVGATTRIBUTE_AUTO */)
{
	OP_ASSERT(attr_type != ATTR_XML); // This will crash if triggered

	SVGAttribute* svg_attr = GetSVGAttr(e, attr_type, ns_idx, is_special);
	if (svg_attr != NULL)
	{
		SVGObject* svg_object = svg_attr->GetSVGObject(field_type, anim_attribute_type);

		if (svg_object && (svg_object->Type() == object_type ||
						   object_type == SVGOBJECT_UNKNOWN) &&
			!svg_object->Flag(SVGOBJECTFLAG_UNINITIALIZED))
		{
			*obj = svg_object;
			return svg_object->HasError() ? (OP_STATUS)OpSVGStatus::INVALID_ARGUMENT : OpStatus::OK;
		}
	}
	*obj = NULL;
	return OpStatus::OK;
}

/* static */ OP_STATUS
AttrValueStore::SetBaseObject(HTML_Element *e, Markup::AttrType attr_name, int ns_idx, BOOL is_special, SVGObject* obj)
{
	OP_ASSERT(obj != NULL);
	OP_ASSERT(e != NULL);

#ifdef _DEBUG
	OP_ASSERT(e->GetNsType() == NS_SVG);
	NS_Type attr_ns = g_ns_manager->GetNsTypeAt(e->ResolveNsIdx(ns_idx));
	OP_ASSERT(attr_ns == NS_SVG || attr_ns == NS_XLINK);
#endif // _DEBUG

	SVGAttribute* svg_attr = GetSVGAttr(e, attr_name, ns_idx, is_special);
	if (svg_attr == NULL)
	{
		svg_attr = OP_NEW(SVGAttribute, (obj));
		if (!svg_attr)
		{
			OP_DELETE(obj);
			return OpStatus::ERR_NO_MEMORY;
		}

		if (is_special)
		{
			OP_ASSERT(Markup::IsSpecialAttribute(attr_name));
			e->SetSpecialAttr(attr_name, ITEM_TYPE_COMPLEX, svg_attr, TRUE, static_cast<SpecialNs::Ns>(ns_idx)); 
		}
		else
		{
			OP_ASSERT(!Markup::IsSpecialAttribute(attr_name));
			e->SetAttr(attr_name, ITEM_TYPE_COMPLEX, svg_attr, TRUE, ns_idx); 
		}
	}
	else
	{
		svg_attr->ReplaceBaseObject(obj);
	}

	return OpStatus::OK;
}

#if 0
/* static */ UINT32
AttrValueStore::GetSerial(HTML_Element *e, int attr_name, int ns)
{
	SVGAttribute* svg_attr = (SVGAttribute*) e->GetAttr(attr_name, ITEM_TYPE_SVGOBJECT, NULL, ns);
	if (svg_attr != NULL)
		return svg_attr->GetSerial();
	return 0;
}
#endif // 0

OP_STATUS
AttrValueStore::GetProxyObject(HTML_Element *e, Markup::AttrType type, SVGProxyObject **val)
{
	SVGObject *obj;
	OP_ASSERT(val);
	RETURN_IF_ERROR(GetObject(e, type, SVGOBJECT_PROXY, &obj));

	*val = static_cast<SVGProxyObject*>(obj);
	return OpStatus::OK;
}

OP_STATUS
AttrValueStore::GetLength(HTML_Element *e, Markup::AttrType type, SVGLengthObject **val, SVGLengthObject *def)
{
	SVGObject *v;
	OP_ASSERT(val);
	RETURN_IF_ERROR(GetObject(e, type, SVGOBJECT_LENGTH, &v));
	if (v != NULL)
		*val = static_cast<SVGLengthObject*>(v);
	else
		*val = def;
	return OpStatus::OK;
}

OP_STATUS
AttrValueStore::GetNumberObject(HTML_Element *e, Markup::AttrType type, SVGNumberObject **val)
{
	SVGObject *v;
	OP_ASSERT(val);
	RETURN_IF_ERROR(GetObject(e, type, SVGOBJECT_NUMBER, &v));

	*val = static_cast<SVGNumberObject*>(v);
	return OpStatus::OK;
}

OP_STATUS
AttrValueStore::GetString(HTML_Element *e, Markup::AttrType type, SVGString **val)
{
	SVGObject *obj;
	OP_ASSERT(val);
	RETURN_IF_ERROR(GetObject(e, type, SVGOBJECT_STRING, &obj));

	*val = static_cast<SVGString*>(obj);
	return OpStatus::OK;
}

OP_STATUS
AttrValueStore::GetNumber(HTML_Element *e, Markup::AttrType type, SVGNumber &val, SVGNumber def)
{
	SVGNumberObject* number_value;
	RETURN_IF_ERROR(GetNumberObject(e, type, &number_value));
	if (number_value != NULL)
		val = number_value->value;
	else
		val = def;
	return OpStatus::OK;
}

void
AttrValueStore::GetVector(HTML_Element *e, Markup::AttrType type, SVGVector*& vector, SVGAttributeField field_type /* = SVG_ATTRFIELD_DEFAULT */)
{
	vector = NULL;

	SVGObject* obj;
	
	OP_STATUS err = GetObject(e, type, SVGOBJECT_VECTOR, &obj, field_type);
	if (OpStatus::IsSuccess(err))
	{
		vector = static_cast<SVGVector*>(obj);
	}
}

/* static */ OP_STATUS
AttrValueStore::GetVectorWithStatus(HTML_Element *e, Markup::AttrType type, SVGVector*& list,
									SVGAttributeField field_type /* = SVG_ATTRFIELD_DEFAULT */)
{
	SVGObject* obj;
	OP_STATUS status = GetObject(e, type, SVGOBJECT_VECTOR, &obj, field_type);
	list = static_cast<SVGVector*>(obj);
	return status;
}

void
AttrValueStore::GetMatrix(HTML_Element* e, Markup::AttrType type, SVGMatrix& matrix, SVGAttributeField field_type /* = SVG_ATTRFIELD_DEFAULT */)
{
	SVGObject* obj;
	BOOL is_special = Markup::IsSpecialAttribute(type);
	if (is_special)
		GetObject(e, type, SpecialNs::NS_SVG, TRUE, SVGOBJECT_UNKNOWN, &obj, field_type);
	else
		GetObject(e, type, SVGOBJECT_UNKNOWN, &obj, field_type); // Ignore return value, we check 'obj'

	if (obj)
	{
		if (obj->Type() == SVGOBJECT_MATRIX)
		{
			matrix.Copy(static_cast<SVGMatrixObject*>(obj)->matrix);
		}
		else if (obj->Type() == SVGOBJECT_VECTOR)
		{
			SVGVector* lst = static_cast<SVGVector*>(obj);
			lst->GetMatrix(matrix);
		}
		else if (obj->Type() == SVGOBJECT_TRANSFORM)
		{
			static_cast<SVGTransform *>(obj)->GetMatrix(matrix);
		}
		else
		{
			matrix.LoadIdentity();
		}
	}
	else
	{
		matrix.LoadIdentity();
	}

	if ((type == Markup::SVGA_PATTERNTRANSFORM || type == Markup::SVGA_GRADIENTTRANSFORM) &&
		e->HasSpecialAttr(Markup::SVGA_ANIMATE_TRANSFORM, SpecialNs::NS_SVG))
	{
		BOOL additive = ATTR_VAL_AS_NUM(e->GetSpecialAttr(Markup::SVGA_ANIMATE_TRANSFORM_ADDITIVE, ITEM_TYPE_NUM,
														  NUM_AS_ATTR_VAL(TRUE), SpecialNs::NS_SVG)) != FALSE;
		SVGMatrix animation_matrix;
		GetMatrix(e, Markup::SVGA_ANIMATE_TRANSFORM, animation_matrix);
		if (additive)
		{
			matrix.PostMultiply(animation_matrix);
		}
		else
		{
			matrix.Copy(animation_matrix);
		}
	}
}

OP_STATUS AttrValueStore::GetViewBox(HTML_Element *e, SVGRectObject **viewbox)
{
	SVGObject* obj;
	OP_ASSERT(viewbox);
	RETURN_IF_ERROR(GetObject(e, Markup::SVGA_VIEWBOX, SVGOBJECT_RECT, &obj));

	*viewbox = static_cast<SVGRectObject*>(obj);
	return OpStatus::OK;
}

OP_STATUS AttrValueStore::GetFontSize(HTML_Element *e, SVGFontSize &font_size)
{
	SVGFontSizeObject *font_size_object;
	RETURN_IF_ERROR(GetFontSizeObject(e, font_size_object));

	if (font_size_object != NULL)
		font_size = font_size_object->font_size;
	else
		font_size = SVGFontSize(SVGFontSize::MODE_UNKNOWN);
	return OpStatus::OK;
}

/* static */ OP_STATUS
AttrValueStore::GetFontSizeObject(HTML_Element *e, SVGFontSizeObject *&font_size_object)
{
	SVGObject* obj;
	RETURN_IF_ERROR(GetObject(e, Markup::SVGA_FONT_SIZE, SVGOBJECT_FONTSIZE, &obj));

	font_size_object = static_cast<SVGFontSizeObject *>(obj);
	return OpStatus::OK;
}

OP_STATUS AttrValueStore::GetEnumObject(HTML_Element *e, Markup::AttrType attr_type, SVGEnumType enum_type, SVGEnum** enum_obj)
{
	SVGObject* obj;
	RETURN_IF_ERROR(GetObject(e, attr_type, SVGOBJECT_ENUM, &obj));
	if (obj != NULL)
	{
		SVGEnum* enum_value = static_cast<SVGEnum*>(obj);
		if (enum_value->EnumType() == enum_type)
		{
			*enum_obj = enum_value;
			return OpStatus::OK;
		}
	}
	*enum_obj = NULL;
	return OpStatus::OK;
}

/* static */ int
AttrValueStore::GetEnumValue(HTML_Element *e, Markup::AttrType attr_type, SVGEnumType enum_type, int default_value)
{
	SVGObject* obj;
	RETURN_VALUE_IF_ERROR(GetObject(e, attr_type, SVGOBJECT_ENUM, &obj), default_value);
	OP_ASSERT(!obj || obj->Type() == SVGOBJECT_ENUM);
	if (obj != NULL && static_cast<SVGEnum *>(obj)->EnumType() == enum_type)
	{
		return static_cast<SVGEnum *>(obj)->EnumValue();
	}
	else
	{
		return default_value;
	}
}

/* static */ OP_STATUS
AttrValueStore::GetAnimationTime(HTML_Element *e,
								 Markup::AttrType type,
								 SVG_ANIMATION_TIME& animation_time,
								 SVG_ANIMATION_TIME default_animation_time)
{
	SVGObject* obj = NULL;
	RETURN_IF_ERROR(GetObject(e, type, SVGOBJECT_ANIMATION_TIME, &obj));
	if (obj != NULL)
		animation_time = static_cast<SVGAnimationTimeObject *>(obj)->value;
	else
		animation_time = default_animation_time;
	return OpStatus::OK;
}

OP_STATUS AttrValueStore::GetXLinkHREF(URL root_url, HTML_Element *e, URL*& outurl, SVGAttributeField field_type /* = SVG_ATTRFIELD_DEFAULT */, BOOL use_unreliable_cached /* = FALSE */)
{
	SVGObject* obj = NULL;
	RETURN_IF_ERROR(GetObject(e, Markup::XLINKA_HREF, NS_IDX_XLINK, FALSE, SVGOBJECT_URL, &obj, field_type));
	if (obj != NULL)
	{
		SVGURL* url = static_cast<SVGURL *>(obj);
		if (use_unreliable_cached)
		{
			OP_ASSERT(root_url.IsEmpty()); // Else there is no reason to use the cached one
			outurl = url->GetUnreliableCachedURLPtr();
		}
		else
		{
			outurl = url->GetURLPtr(root_url, e);
		}
	}
	else
	{
		outurl = NULL;
	}

	return OpStatus::OK;
}

OP_STATUS AttrValueStore::GetNavRefObject(HTML_Element *e, Markup::AttrType attr, SVGNavRef*& navref_object)
{
	SVGObject* obj;
	RETURN_IF_ERROR(GetObject(e, attr, SVGOBJECT_NAVREF, &obj));

	navref_object = static_cast<SVGNavRef *>(obj);
	return OpStatus::OK;
}

SVGAnimationInterface* AttrValueStore::GetSVGAnimationInterface(HTML_Element *e)
{
	OP_NEW_DBG("AttrValueStore::GetSVGAnimationInterface", "svg_context");
	if (!e || e->GetNsType() != NS_SVG)
		return NULL;

	SVGElementContext *ctx = static_cast<SVGElementContext*>(e->GetSVGContext());
	if (ctx != NULL)
	{
		if (SVGAnimationInterface *animctx = ctx->GetAnimationInterface())
			return animctx;

#ifdef _DEBUG
		OP_DBG(("Warning: Wrong type of SVG Context. Expected SVGAnimationInterface to be present."));
#endif // _DEBUG
	}

	return NULL;
}

SVGTimingInterface* AttrValueStore::GetSVGTimingInterface(HTML_Element *e)
{
	OP_NEW_DBG("AttrValueStore::GetSVGTimingInterface", "svg_context");

	if (!e || e->GetNsType() != NS_SVG)
		return NULL;

	SVGElementContext *ctx = static_cast<SVGElementContext*>(e->GetSVGContext());
	if (ctx != NULL)
	{
		if (SVGTimingInterface *timed_element_ctx = ctx->GetTimingInterface())
			return timed_element_ctx;

#ifdef _DEBUG
		OP_DBG(("Warning: Wrong type of SVG Context. Expected SVGTimingInterface to be present."));
#endif // _DEBUG
	}

	return NULL;
}

SVGElementContext* AttrValueStore::AssertSVGElementContext(HTML_Element *e)
{
	OP_NEW_DBG("AttrValueStore::AssertSVGElementContext", "svg_context");
	OP_ASSERT(e && (e->IsText() || e->GetNsType() == NS_SVG));

	if (!e || (!e->IsText() && e->GetNsType() != NS_SVG))
		return NULL;

	SVGElementContext *ctx = static_cast<SVGElementContext*>(e->GetSVGContext());
	if (ctx != NULL)
		return ctx;

	if (!e->IsMatchingType(Markup::SVGE_SVG, NS_SVG))
		return SVGElementContext::Create(e);

	// else we should have had an SVGDocumentContext and if we
	// didn't have one something is really wrong and we might as
	// well bail with an oom (which probably occurred when the
	// Markup::SVGE_SVG were created)
	return NULL;
}

SVGFontElement* AttrValueStore::GetSVGFontElement(HTML_Element *e, BOOL create)
{
	OP_NEW_DBG("AttrValueStore::GetSVGFontElement", "svg_context");

	OP_ASSERT(e);
	OP_ASSERT(e->IsMatchingType(Markup::SVGE_FONT, NS_SVG) || e->IsMatchingType(Markup::SVGE_FONT_FACE, NS_SVG));

	if (e->GetNsType() != NS_SVG)
		return NULL;

	SVGElementContext *elm_ctx = static_cast<SVGElementContext*>(e->GetSVGContext());
	if (create && !elm_ctx)
		elm_ctx = SVGElementContext::Create(e);

	return elm_ctx ? elm_ctx->GetAsFontElement() : NULL;
}

SVGDocumentContext* AttrValueStore::GetSVGDocumentContext(HTML_Element *e)
{
	OP_NEW_DBG("AttrValueStore::GetSVGDocumentContext", "svg_context");
	if (e == NULL)
		return NULL;

	// Never return nested svg roots as document contexts
	e = SVGUtils::GetTopmostSVGRoot(e);

	if (!e || e->GetNsType() != NS_SVG)
		return NULL;

	SVGElementContext *ctx = static_cast<SVGElementContext*>(e->GetSVGContext());
	if (ctx != NULL)
	{
		SVGDocumentContext *doc_ctx = ctx->GetAsDocumentContext();
		if (doc_ctx)
			return doc_ctx;
#ifdef _DEBUG
		else
		{
			OP_DBG(("Warning: Wrong type of SVG Context. Expected SVGDocumentContext."));
			return NULL;
		}
#endif // _DEBUG
	}

	return NULL;
}

OP_STATUS AttrValueStore::GetVisibility(HTML_Element *e, SVGVisibilityType &type,
										BOOL* css, BOOL* is_inherit)
{
	SVGEnum* obj;
	RETURN_IF_ERROR(GetEnumObject(e, Markup::SVGA_VISIBILITY, SVGENUM_VISIBILITY, &obj));
	if (obj != NULL)
	{
		type = (SVGVisibilityType)obj->EnumValue();
		if (css)
			*css = obj->Flag(SVGOBJECTFLAG_IS_CSSPROP);
		if (is_inherit)
			*is_inherit = obj->Flag(SVGOBJECTFLAG_INHERIT);
		return OpStatus::OK;
	}
	return OpStatus::ERR;
}

OP_STATUS AttrValueStore::GetCursor(HTML_Element *e, CursorType &cursor_type, BOOL* css)
{
	SVGObject* obj;
	RETURN_IF_ERROR(GetObject(e, Markup::SVGA_CURSOR, SVGOBJECT_VECTOR, &obj));
	if (obj != NULL)
	{
		SVGVector* vector = static_cast<SVGVector*>(obj);
		for (unsigned int i = 0; i < vector->GetCount(); i++)
		{
			SVGEnum* enum_value = static_cast<SVGEnum*>(vector->Get(i));
			if (enum_value != NULL)
			{
				cursor_type = (CursorType)enum_value->EnumValue();
				if (css)
					*css = obj->Flag(SVGOBJECTFLAG_IS_CSSPROP);
				return OpStatus::OK;
			}
		}
	}
	OP_ASSERT(0);
	return OpStatus::ERR;
}

OP_STATUS AttrValueStore::GetDisplay(HTML_Element *e, SVGDisplayType &type, BOOL *css)
{
	SVGEnum* obj;
	RETURN_IF_ERROR(GetEnumObject(e, Markup::SVGA_DISPLAY, SVGENUM_DISPLAY, &obj));
	if (obj != NULL)
	{
		type = (SVGDisplayType)obj->EnumValue();
		if (css)
			*css = obj->Flag(SVGOBJECTFLAG_IS_CSSPROP);
		return OpStatus::OK;
	}
	return OpStatus::ERR;
}

OP_STATUS AttrValueStore::GetPreserveAspectRatio(HTML_Element *e, SVGAspectRatio*& ratio)
{
	SVGObject* obj;
	RETURN_IF_ERROR(GetObject(e, Markup::SVGA_PRESERVEASPECTRATIO, SVGOBJECT_ASPECT_RATIO, &obj));

	ratio = static_cast<SVGAspectRatio *>(obj);
	return OpStatus::OK;
}

OP_STATUS AttrValueStore::GetUnits(HTML_Element *e, Markup::AttrType attr, SVGUnitsType &type, SVGUnitsType default_value)
{
	SVGEnum* obj;
	type = default_value;

	RETURN_IF_ERROR(GetEnumObject(e, attr, SVGENUM_UNITS_TYPE, &obj));

	if (obj && (SVGUnitsType)obj->EnumValue() != SVGUNITS_UNKNOWN)
		type = (SVGUnitsType)obj->EnumValue();

	return OpStatus::OK;
}

OP_STATUS AttrValueStore::GetOrientObject(HTML_Element *e, SVGOrient*& orient_object)
{
	SVGObject* obj;
	RETURN_IF_ERROR(GetObject(e, Markup::SVGA_ORIENT, SVGOBJECT_ORIENT, &obj));

	orient_object = static_cast<SVGOrient *>(obj);
	return OpStatus::OK;
}

OP_STATUS AttrValueStore::GetRotateObject(HTML_Element* e, SVGRotate **rotate_object)
{
	SVGObject* obj;
	OP_ASSERT(rotate_object);
	RETURN_IF_ERROR(GetObject(e, Markup::SVGA_ROTATE, SVGOBJECT_ROTATE, &obj));

	*rotate_object = static_cast<SVGRotate *>(obj);
	return OpStatus::OK;
}

OP_STATUS
AttrValueStore::CreateDefaultAttributeObject(HTML_Element* elm, Markup::AttrType attr_name, int ns_idx, BOOL is_special, SVGObject*& base_result)
{
	Markup::Type elm_type = elm->Type();
	NS_Type attr_ns = (is_special ? NS_SPECIAL : g_ns_manager->GetNsTypeAt(elm->ResolveNsIdx(ns_idx)));
	SVGObjectType object_type = SVGUtils::GetObjectType(elm_type,
														attr_name,
														attr_ns);
	if (object_type == SVGOBJECT_UNKNOWN)
		return OpStatus::ERR;

	if (object_type == SVGOBJECT_LENGTH)
	{
		if (elm_type == Markup::SVGE_SVG && (attr_name == Markup::SVGA_WIDTH || attr_name == Markup::SVGA_HEIGHT))
		{
			base_result = OP_NEW(SVGLengthObject, (100, CSS_PERCENTAGE));
		}
		else if (attr_name == Markup::SVGA_TEXTLENGTH && SVGUtils::IsTextClassType(elm_type))
		{
			// tried to get this part of the spec changed, thank mozilla and microsoft for the following part
			SVGTextData data(SVGTextData::EXTENT);
			SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(elm);
			SVGNumberPair viewport;
			if (OpStatus::IsSuccess(SVGUtils::GetViewportForElement(elm, doc_ctx, viewport)))
			{
				if (doc_ctx->GetSVGImage()->IsInTree())
					// If we can't get the extent, let's just use 0 instead.
					OpStatus::Ignore(SVGUtils::GetTextElementExtent(elm, doc_ctx, 0, -1, data,
																	viewport, NULL));
			}
			base_result = OP_NEW(SVGLengthObject, (data.extent));
		}
		else
		{
			RETURN_IF_ERROR(SVGObject::Make(base_result, object_type));
		}
	}
	else if (object_type == SVGOBJECT_ENUM)
	{
		SVGEnumType enum_type = SVGUtils::GetEnumType(elm_type, attr_name);
		if (enum_type == SVGENUM_UNITS_TYPE)
		{
			SVGUnitsType unit_type;
			if (attr_name == Markup::SVGA_CLIPPATHUNITS ||
				attr_name == Markup::SVGA_MASKCONTENTUNITS ||
				attr_name == Markup::SVGA_PATTERNCONTENTUNITS ||
				attr_name == Markup::SVGA_PRIMITIVEUNITS)
				unit_type = SVGUNITS_USERSPACEONUSE;
			else
				unit_type = SVGUNITS_OBJECTBBOX;

			base_result = OP_NEW(SVGEnum, (enum_type, unit_type));
		}
		else if (enum_type == SVGENUM_MARKER_UNITS)
		{
			base_result = OP_NEW(SVGEnum, (enum_type, SVGMARKERUNITS_STROKEWIDTH));
		}
		else if (attr_name == Markup::SVGA_ZOOMANDPAN && (elm_type == Markup::SVGE_SVG || elm_type == Markup::SVGE_VIEW))
		{
			base_result = OP_NEW(SVGEnum, (SVGENUM_ZOOM_AND_PAN, SVGZOOMANDPAN_MAGNIFY));
		}
		else
		{
			RETURN_IF_ERROR(SVGObject::Make(base_result, object_type));
		}
	}
	else if (object_type == SVGOBJECT_VECTOR)
	{
		SVGObjectType vector_type = SVGUtils::GetVectorObjectType(elm_type,
																  attr_name);
		base_result = SVGUtils::CreateSVGVector(vector_type,
											 elm_type,
											 attr_name);
		if (!base_result)
			return OpStatus::ERR_NO_MEMORY;

		if (attr_name == Markup::SVGA_ORDER ||
			attr_name == Markup::SVGA_STDDEVIATION ||
			attr_name == Markup::SVGA_RADIUS ||
			attr_name == Markup::SVGA_BASEFREQUENCY)
		{
			// number-optional-number is special.
			SVGNumber default_item(0);
			if (attr_name == Markup::SVGA_ORDER)
			{
				default_item = SVGNumber(3);
			}
			SVGVector* base_vector = static_cast<SVGVector*>(base_result);
			SVGNumberObject* nval = OP_NEW(SVGNumberObject, (default_item));
			if (!nval || OpStatus::IsMemoryError(base_vector->Append(nval)))
			{
				OP_DELETE(base_result);
				return OpStatus::ERR_NO_MEMORY;
			}
		}
	}
	else
	{
		RETURN_IF_ERROR(SVGObject::Make(base_result, object_type));
	}

	return (base_result != NULL) ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

/* static */ OP_STATUS
AttrValueStore::GetRepeatCount(HTML_Element *e, SVGRepeatCount &repeat_count)
{
	SVGObject* obj;
	RETURN_IF_ERROR(GetObject(e, Markup::SVGA_REPEATCOUNT, SVGOBJECT_REPEAT_COUNT, &obj));
	if (obj != NULL)
	{
		repeat_count = static_cast<SVGRepeatCountObject *>(obj)->repeat_count;

		// If the repeatCount is less than or equal to zero, treat it
		// as unspecified.
		if (repeat_count.IsValue() && repeat_count.value <= 0)
			repeat_count = SVGRepeatCount();
	}
	else
	{
		repeat_count = SVGRepeatCount(); // Means unspecified
	}
	return OpStatus::OK;
}

/* static */ SVGAttribute *
AttrValueStore::CreateAttribute(HTML_Element *element, Markup::AttrType attr_name, int ns_idx, BOOL is_special, SVGObject *base_object)
{
	OP_ASSERT(base_object != NULL);
	OP_ASSERT(element != NULL);

#ifdef _DEBUG
	NS_Type attr_ns = g_ns_manager->GetNsTypeAt(element->ResolveNsIdx(ns_idx));
	OP_ASSERT(element->GetNsType() == NS_SVG);
	OP_ASSERT(attr_ns == NS_SVG || attr_ns == NS_XLINK);
#endif // _DEBUG

	OP_ASSERT(GetSVGAttr(element, attr_name, ns_idx) == NULL);

	SVGAttribute *svg_attr = OP_NEW(SVGAttribute, (base_object));
	if (!svg_attr)
	{
		return NULL;
	}

	if (is_special)
	{
		OP_ASSERT(Markup::IsSpecialAttribute(attr_name));
		if (element->SetSpecialAttr(attr_name, ITEM_TYPE_COMPLEX, svg_attr, TRUE, static_cast<SpecialNs::Ns>(ns_idx)) < 0)
		{
			OP_DELETE(svg_attr);
			return NULL;
		}
	}
	else
	{
		OP_ASSERT(!Markup::IsSpecialAttribute(attr_name));
		if (element->SetAttr(attr_name, ITEM_TYPE_COMPLEX, svg_attr, TRUE, ns_idx) < 0)
		{
			OP_DELETE(svg_attr);
			return NULL;
		}
	}

	return svg_attr;
}

/* static */ BOOL
AttrValueStore::HasRefTransform(HTML_Element* e, Markup::AttrType type, SVGMatrix& matrix)
{
	SVGObject* obj;
	GetObject(e, type, SVGOBJECT_UNKNOWN, &obj, SVG_ATTRFIELD_DEFAULT); // Ignore return value, we check 'obj'
	if (obj && obj->Type() == SVGOBJECT_VECTOR)
	{
		SVGVector* lst = static_cast<SVGVector*>(obj);
		if (lst->IsRefTransform())
		{
			lst->GetMatrix(matrix);
			return TRUE;
		}
	}
	return FALSE;
}

/* static */ SVGObject*
AttrValueStore::GetPresentationAttributeForDOM(HTML_Element *element, Markup::AttrType attr_type,
											   SVGObjectType object_type)
{
	if (SVGAttribute* svg_attr = GetSVGAttr(element, attr_type, NS_IDX_SVG))
	{
		SVGObject* svg_object = svg_attr->GetSVGObject(SVG_ATTRFIELD_BASE, SVGATTRIBUTE_AUTO);

		if (svg_object && !svg_object->Flag(SVGOBJECTFLAG_UNINITIALIZED))
		{
			if (svg_object->Type() == object_type ||
				object_type == SVGOBJECT_UNKNOWN)
			{
				// Invalidate cached string representation
				svg_attr->ClearOverrideString();
				return svg_object;
			}
		}
	}
	return NULL;
}

/* static */ OP_STATUS
AttrValueStore::GetAttributeObjectsForDOM(HTML_Element *element, Markup::AttrType attr_type,
										  int ns_idx, SVGObject **base_obj,
										  SVGObject **anim_obj)
{
	SVGAttribute* svg_attr = GetSVGAttr(element, attr_type, ns_idx);
	if (svg_attr == NULL)
	{
		SVGObject* obj;
		RETURN_IF_ERROR(CreateDefaultAttributeObject(element, attr_type,
													 ns_idx, FALSE, obj));

		/** We don't want to read this value when drawing since we
		 * can't make up default attributes at every attribute/element
		 * combination. */
		obj->SetFlag(SVGOBJECTFLAG_UNINITIALIZED);
		svg_attr = CreateAttribute(element, attr_type, ns_idx, FALSE, obj);
		if (svg_attr == NULL)
		{
			OP_DELETE(obj);
			return OpStatus::ERR_NO_MEMORY;
		}
	}
	else
	{
		// If DOM has gotten a hold of the objects, we can't use our
		// cached string representation anymore
		svg_attr->ClearOverrideString();
	}

	if (base_obj)
	{
		*base_obj = svg_attr->GetSVGObject(SVG_ATTRFIELD_BASE, SVGATTRIBUTE_AUTO);
	}

	if (anim_obj)
	{
		NS_Type ns = g_ns_manager->GetNsTypeAt(element->ResolveNsIdx(ns_idx));

		if (attr_type == Markup::SVGA_TRANSFORM && ns == NS_SVG)
		{
			GetObject(element, Markup::SVGA_ANIMATE_TRANSFORM, SpecialNs::NS_SVG, TRUE,
					  SVGOBJECT_UNKNOWN, anim_obj,
					  SVG_ATTRFIELD_ANIM); // Ignore parse errors
		}
		else
		{
			*anim_obj = svg_attr->GetSVGObject(SVG_ATTRFIELD_ANIM, SVGATTRIBUTE_AUTO);
		}
	}

	// If the animation value is missing, use the base value as
	// animation value also.
	if (base_obj != NULL && anim_obj != NULL &&
		*base_obj && !*anim_obj)
	{
		*anim_obj = *base_obj;
	}

	return OpStatus::OK;
}

#endif // SVG_SUPPORT
