/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef SVG_DOM
#include "modules/dom/src/domglobaldata.h"
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domsvg/domsvgobject.h"
#include "modules/dom/src/domsvg/domsvglist.h"
#include "modules/dom/src/domsvg/domsvgobjectstore.h"

#include "modules/doc/frm_doc.h"

#include "modules/svg/SVGManager.h"
#include "modules/svg/svg.h"
#include "modules/svg/svg_path.h"

#ifdef DOM_NO_COMPLEX_GLOBALS
# define DOM_SVG_OBJECT_PROTOTYPE_CLASSNAMES_START() void DOM_svgObjectPrototypeClassNames_Init(DOM_GlobalData *global_data) { const char **classnames = global_data->svgObjectPrototypeClassNames;
# define DOM_SVG_OBJECT_PROTOTYPE_CLASSNAMES_ITEM(name) *classnames = "SVG" name "Prototype"; ++classnames;
# define DOM_SVG_OBJECT_PROTOTYPE_CLASSNAMES_ITEM_LAST(name) *classnames = "SVG" name "Prototype";
# define DOM_SVG_OBJECT_PROTOTYPE_CLASSNAMES_END() }
#else // DOM_NO_COMPLEX_GLOBALS
# define DOM_SVG_OBJECT_PROTOTYPE_CLASSNAMES_START() const char *const g_DOM_svgObjectPrototypeClassNames[] = {
# define DOM_SVG_OBJECT_PROTOTYPE_CLASSNAMES_ITEM(name) "SVG" name "Prototype",
# define DOM_SVG_OBJECT_PROTOTYPE_CLASSNAMES_ITEM_LAST(name) "SVG" name "Prototype"
# define DOM_SVG_OBJECT_PROTOTYPE_CLASSNAMES_END() };
#endif // DOM_NO_COMPLEX_GLOBALS

/* Important: this array must be kept in sync with the enumeration
   DOM_Runtime::SVGObjectPrototype.  See its definition for what
   else (if anything) it needs to be in sync with. */
DOM_SVG_OBJECT_PROTOTYPE_CLASSNAMES_START()
	DOM_SVG_OBJECT_PROTOTYPE_CLASSNAMES_ITEM("Number")
	DOM_SVG_OBJECT_PROTOTYPE_CLASSNAMES_ITEM("Length")
	DOM_SVG_OBJECT_PROTOTYPE_CLASSNAMES_ITEM("Point")
	DOM_SVG_OBJECT_PROTOTYPE_CLASSNAMES_ITEM("Angle")
	DOM_SVG_OBJECT_PROTOTYPE_CLASSNAMES_ITEM("Transform")
	DOM_SVG_OBJECT_PROTOTYPE_CLASSNAMES_ITEM("PathSeg")
	DOM_SVG_OBJECT_PROTOTYPE_CLASSNAMES_ITEM("Matrix")
	DOM_SVG_OBJECT_PROTOTYPE_CLASSNAMES_ITEM("Paint")
	DOM_SVG_OBJECT_PROTOTYPE_CLASSNAMES_ITEM("AspectRatio")
	DOM_SVG_OBJECT_PROTOTYPE_CLASSNAMES_ITEM("CSSPrimitiveValue")
	DOM_SVG_OBJECT_PROTOTYPE_CLASSNAMES_ITEM("Rect")
	DOM_SVG_OBJECT_PROTOTYPE_CLASSNAMES_ITEM("CSSRGBColor")
	DOM_SVG_OBJECT_PROTOTYPE_CLASSNAMES_ITEM("Path")
	DOM_SVG_OBJECT_PROTOTYPE_CLASSNAMES_ITEM_LAST("RGBColor")
DOM_SVG_OBJECT_PROTOTYPE_CLASSNAMES_END()

#undef DOM_SVG_OBJECT_PROTOTYPE_CLASSNAMES_START
#undef DOM_SVG_OBJECT_PROTOTYPE_CLASSNAMES_ITEM
#undef DOM_SVG_OBJECT_PROTOTYPE_CLASSNAMES_ITEM_LAST
#undef DOM_SVG_OBJECT_PROTOTYPE_CLASSNAMES_END

/* static */ OP_STATUS
DOM_SVGObject::Make(DOM_SVGObject *&obj, SVGDOMItem *svg_item, const DOM_SVGLocation& location, DOM_EnvironmentImpl *environment)
{
	OP_ASSERT(svg_item != NULL);
	// These shouldn't get objects
	OP_ASSERT(!svg_item->IsA(SVG_DOM_ITEMTYPE_STRING));
	OP_ASSERT(!svg_item->IsA(SVG_DOM_ITEMTYPE_ENUM));

	DOM_Runtime *runtime = environment->GetDOMRuntime();
	const char *classname = svg_item->GetDOMName();

	obj = OP_NEW(DOM_SVGObject, ());
	if (!obj)
		return OpStatus::ERR_NO_MEMORY;

	obj->location = location;
	obj->svg_item = svg_item;
#ifdef SVG_FULL_11
	obj->in_list = NULL;
#endif // SVG_FULL_11

	/* The order is important here. Must check sub-classes before
	 * base-classes */

	DOM_Runtime::SVGObjectPrototype svg_obj_proto = DOM_Runtime::SVGNUMBER_PROTOTYPE; // Bogus
	int ifs =  SVG_DOM_ITEMTYPE_UNKNOWN;
	if (svg_item->IsA(SVG_DOM_ITEMTYPE_POINT))
	{
		svg_obj_proto = DOM_Runtime::SVGPOINT_PROTOTYPE;
		ifs = SVG_DOM_ITEMTYPE_POINT;
	}
#ifdef SVG_FULL_11
	else if (svg_item->IsA(SVG_DOM_ITEMTYPE_NUMBER))
	{
		svg_obj_proto = DOM_Runtime::SVGNUMBER_PROTOTYPE;
		ifs = SVG_DOM_ITEMTYPE_NUMBER;
	}
	else if (svg_item->IsA(SVG_DOM_ITEMTYPE_LENGTH))
	{
		svg_obj_proto = DOM_Runtime::SVGLENGTH_PROTOTYPE;
		ifs = SVG_DOM_ITEMTYPE_LENGTH;
	}
	else if (svg_item->IsA(SVG_DOM_ITEMTYPE_ANGLE))
	{
		svg_obj_proto = DOM_Runtime::SVGANGLE_PROTOTYPE;
		ifs = SVG_DOM_ITEMTYPE_ANGLE;
	}
	else if (svg_item->IsA(SVG_DOM_ITEMTYPE_PAINT))
	{
		svg_obj_proto = DOM_Runtime::SVGPAINT_PROTOTYPE;
		ifs = SVG_DOM_ITEMTYPE_PAINT;
	}
	else if (svg_item->IsA(SVG_DOM_ITEMTYPE_TRANSFORM))
	{
		svg_obj_proto = DOM_Runtime::SVGTRANSFORM_PROTOTYPE;
		ifs = SVG_DOM_ITEMTYPE_TRANSFORM;
	}
#endif // SVG_FULL_11
	else if (svg_item->IsA(SVG_DOM_ITEMTYPE_MATRIX))
	{
		svg_obj_proto = DOM_Runtime::SVGMATRIX_PROTOTYPE;
		ifs = SVG_DOM_ITEMTYPE_MATRIX;
	}
#ifdef SVG_FULL_11
	else if (svg_item->IsA(SVG_DOM_ITEMTYPE_PATHSEG))
	{
		svg_obj_proto = DOM_Runtime::SVGPATHSEG_PROTOTYPE;
		ifs = SVG_DOM_ITEMTYPE_PATHSEG;
	}
	else if (svg_item->IsA(SVG_DOM_ITEMTYPE_PRESERVE_ASPECT_RATIO))
	{
		svg_obj_proto = DOM_Runtime::SVGASPECTRATIO_PROTOTYPE;
		ifs = SVG_DOM_ITEMTYPE_PRESERVE_ASPECT_RATIO;
	}
	else if (svg_item->IsA(SVG_DOM_ITEMTYPE_CSSPRIMITIVEVALUE))
	{
		svg_obj_proto = DOM_Runtime::SVGCSSPRIMITIVEVALUE_PROTOTYPE;
		ifs = SVG_DOM_ITEMTYPE_PRESERVE_ASPECT_RATIO;
	}
#endif // SVG_FULL_11
	else if (svg_item->IsA(SVG_DOM_ITEMTYPE_RECT))
	{
		svg_obj_proto = DOM_Runtime::SVGRECT_PROTOTYPE;
		ifs = SVG_DOM_ITEMTYPE_RECT;
	}
#ifdef SVG_FULL_11
	else if (svg_item->IsA(SVG_DOM_ITEMTYPE_CSSRGBCOLOR))
	{
		svg_obj_proto = DOM_Runtime::SVGCSSRGBCOLOR_PROTOTYPE;
		ifs = SVG_DOM_ITEMTYPE_CSSRGBCOLOR;
	}
#endif // SVG_FULL_11
#ifdef SVG_TINY_12
	else if (svg_item->IsA(SVG_DOM_ITEMTYPE_PATH))
	{
		svg_obj_proto = DOM_Runtime::SVGPATH_PROTOTYPE;
		ifs = SVG_DOM_ITEMTYPE_PATH;
	}
	else if (svg_item->IsA(SVG_DOM_ITEMTYPE_RGBCOLOR))
	{
		svg_obj_proto = DOM_Runtime::SVGRGBCOLOR_PROTOTYPE;
		ifs = SVG_DOM_ITEMTYPE_RGBCOLOR;
	}
#endif // SVG_TINY_12
	else
	{
		OP_ASSERT(!"Unhandled object type");
	}

	ES_Object *prototype = NULL;
	if (ifs != SVG_DOM_ITEMTYPE_UNKNOWN)
		prototype = runtime->GetSVGObjectPrototype(svg_obj_proto);
	if (!prototype)
		prototype = runtime->GetObjectPrototype();

	RETURN_IF_ERROR(DOM_SVGObjectStore::Make(obj->object_store, g_DOM_svg_object_entries, ifs));
	RETURN_IF_ERROR(DOMSetObjectRuntime(obj, runtime, prototype, classname));

	return OpStatus::OK;
}

#ifdef SVG_FULL_11
ES_GetState
DOM_SVGObject::GetLengthValue(OpAtom property_name, ES_Value* value, SVGDOMLength* svg_length, ES_Runtime* origining_runtime)
{
	if (value)
	{
		double len_value = svg_length->GetValue(GetThisElement(), location.GetAttr(), location.GetNS());
		DOMSetNumber(value, len_value);
	}
	return GET_SUCCESS;
}
#endif // SVG_FULL_11

/* virtual */
DOM_SVGObject::~DOM_SVGObject()
{
#ifdef SVG_FULL_11
	if (in_list)
		in_list->ReleaseObject(this);
#endif // SVG_FULL_11

	OP_DELETE(object_store);
	OP_DELETE(svg_item);
}

/* virtual */ void
DOM_SVGObject::GCTrace()
{
	object_store->GCTrace(GetRuntime());
	location.GCTrace();

#ifdef SVG_FULL_11
	GCMark(in_list);
#endif // SVG_FULL_11
}

/* static */ void
DOM_SVGObject::PutConstructorsL(DOM_Object* target)
{
	DOM_Runtime *runtime = target->GetRuntime();
	DOM_Object* object;

	object = target->PutConstructorL("SVGPoint", DOM_Runtime::SVGPOINT_PROTOTYPE);
	DOM_SVGObject::ConstructDOMImplementationSVGObjectL(*object, SVG_DOM_ITEMTYPE_POINT, runtime);

	object = target->PutConstructorL("SVGMatrix", DOM_Runtime::SVGMATRIX_PROTOTYPE);
	DOM_SVGObject::ConstructDOMImplementationSVGObjectL(*object, SVG_DOM_ITEMTYPE_MATRIX, runtime);

	object = target->PutConstructorL("SVGRect", DOM_Runtime::SVGRECT_PROTOTYPE);
	DOM_SVGObject::ConstructDOMImplementationSVGObjectL(*object, SVG_DOM_ITEMTYPE_RECT, runtime);

#ifdef SVG_FULL_11
	object = target->PutConstructorL("CSSPrimitiveValue", DOM_Runtime::SVGCSSPRIMITIVEVALUE_PROTOTYPE);
	DOM_SVGObject::ConstructDOMImplementationSVGObjectL(*object, SVG_DOM_ITEMTYPE_CSSPRIMITIVEVALUE, runtime);

	object = target->PutConstructorL("RGBColor", DOM_Runtime::SVGCSSRGBCOLOR_PROTOTYPE);
	DOM_SVGObject::ConstructDOMImplementationSVGObjectL(*object, SVG_DOM_ITEMTYPE_CSSRGBCOLOR, runtime);

	object = target->PutConstructorL("SVGNumber", DOM_Runtime::SVGNUMBER_PROTOTYPE);
	DOM_SVGObject::ConstructDOMImplementationSVGObjectL(*object, SVG_DOM_ITEMTYPE_NUMBER, runtime);

	object = target->PutConstructorL("SVGLength", DOM_Runtime::SVGLENGTH_PROTOTYPE);
	DOM_SVGObject::ConstructDOMImplementationSVGObjectL(*object, SVG_DOM_ITEMTYPE_LENGTH, runtime);

	object = target->PutConstructorL("SVGAngle", DOM_Runtime::SVGANGLE_PROTOTYPE);
	DOM_SVGObject::ConstructDOMImplementationSVGObjectL(*object, SVG_DOM_ITEMTYPE_ANGLE, runtime);

	object = target->PutConstructorL("SVGTransform", DOM_Runtime::SVGTRANSFORM_PROTOTYPE);
	DOM_SVGObject::ConstructDOMImplementationSVGObjectL(*object, SVG_DOM_ITEMTYPE_TRANSFORM, runtime);

	object = target->PutConstructorL("SVGPaint", DOM_Runtime::SVGPAINT_PROTOTYPE);
	DOM_SVGObject::ConstructDOMImplementationSVGObjectL(*object, SVG_DOM_ITEMTYPE_PAINT, runtime);

	object = target->PutConstructorL("SVGPreserveAspectRatio", DOM_Runtime::SVGASPECTRATIO_PROTOTYPE);
	DOM_SVGObject::ConstructDOMImplementationSVGObjectL(*object, SVG_DOM_ITEMTYPE_PRESERVE_ASPECT_RATIO, runtime);

	object = target->PutConstructorL("SVGPathSeg", DOM_Runtime::SVGPATHSEG_PROTOTYPE);
	DOM_SVGObject::ConstructDOMImplementationSVGObjectL(*object, SVG_DOM_ITEMTYPE_PATHSEG, runtime);
#endif // SVG_FULL_11

#ifdef SVG_TINY_12
	object = target->PutConstructorL("SVGRGBColor", DOM_Runtime::SVGRGBCOLOR_PROTOTYPE);
	DOM_SVGObject::ConstructDOMImplementationSVGObjectL(*object, SVG_DOM_ITEMTYPE_RGBCOLOR, runtime);

	object = target->PutConstructorL("SVGPath", DOM_Runtime::SVGPATH_PROTOTYPE);
	DOM_SVGObject::ConstructDOMImplementationSVGObjectL(*object, SVG_DOM_ITEMTYPE_PATH, runtime);
#endif // SVG_TINY_12
}

/* static */ void
DOM_SVGObject::InitializeConstructorsTableL(OpString8HashTable<DOM_ConstructorInformation> *table)
{
#define ADD_CONSTRUCTOR(name, id) table->AddL(#name, OP_NEW_L(DOM_ConstructorInformation, (#name, DOM_Runtime::CTN_SVGOBJECT, DOM_Runtime::id##_PROTOTYPE)))

	ADD_CONSTRUCTOR(SVGPoint, SVGPOINT);
	ADD_CONSTRUCTOR(SVGMatrix, SVGMATRIX);
	ADD_CONSTRUCTOR(SVGRect, SVGRECT);

#ifdef SVG_FULL_11
	ADD_CONSTRUCTOR(CSSPrimitiveValue, SVGCSSPRIMITIVEVALUE);
	ADD_CONSTRUCTOR(RGBColor, SVGCSSRGBCOLOR);
	ADD_CONSTRUCTOR(SVGNumber, SVGNUMBER);
	ADD_CONSTRUCTOR(SVGLength, SVGLENGTH);
	ADD_CONSTRUCTOR(SVGAngle, SVGANGLE);
	ADD_CONSTRUCTOR(SVGTransform, SVGTRANSFORM);
	ADD_CONSTRUCTOR(SVGPaint, SVGPAINT);
	ADD_CONSTRUCTOR(SVGPreserveAspectRatio, SVGASPECTRATIO);
	ADD_CONSTRUCTOR(SVGPathSeg, SVGPATHSEG);
#endif // SVG_FULL_11

#ifdef SVG_TINY_12
	ADD_CONSTRUCTOR(SVGRGBColor, SVGRGBCOLOR);

	ADD_CONSTRUCTOR(SVGPath, SVGPATH);
#endif // SVG_TINY_12
#undef ADD_CONSTRUCTOR
}

/* static */ ES_Object *
DOM_SVGObject::CreateConstructorL(DOM_Runtime *runtime, DOM_Object *target, const char *name, unsigned id)
{
	DOM_Object *object = target->PutConstructorL(name, static_cast<DOM_Runtime::SVGObjectPrototype>(id));

	switch (id)
	{
	case DOM_Runtime::SVGPOINT_PROTOTYPE:
		DOM_SVGObject::ConstructDOMImplementationSVGObjectL(*object, SVG_DOM_ITEMTYPE_POINT, runtime);
		break;

	case DOM_Runtime::SVGMATRIX_PROTOTYPE:
		DOM_SVGObject::ConstructDOMImplementationSVGObjectL(*object, SVG_DOM_ITEMTYPE_MATRIX, runtime);
		break;

	case DOM_Runtime::SVGRECT_PROTOTYPE:
		DOM_SVGObject::ConstructDOMImplementationSVGObjectL(*object, SVG_DOM_ITEMTYPE_RECT, runtime);
		break;

#ifdef SVG_FULL_11
	case DOM_Runtime::SVGCSSPRIMITIVEVALUE_PROTOTYPE:
		DOM_SVGObject::ConstructDOMImplementationSVGObjectL(*object, SVG_DOM_ITEMTYPE_CSSPRIMITIVEVALUE, runtime);
		break;

	case DOM_Runtime::SVGCSSRGBCOLOR_PROTOTYPE:
		DOM_SVGObject::ConstructDOMImplementationSVGObjectL(*object, SVG_DOM_ITEMTYPE_CSSRGBCOLOR, runtime);
		break;

	case DOM_Runtime::SVGNUMBER_PROTOTYPE:
		DOM_SVGObject::ConstructDOMImplementationSVGObjectL(*object, SVG_DOM_ITEMTYPE_NUMBER, runtime);
		break;

	case DOM_Runtime::SVGLENGTH_PROTOTYPE:
		DOM_SVGObject::ConstructDOMImplementationSVGObjectL(*object, SVG_DOM_ITEMTYPE_LENGTH, runtime);
		break;

	case DOM_Runtime::SVGANGLE_PROTOTYPE:
		DOM_SVGObject::ConstructDOMImplementationSVGObjectL(*object, SVG_DOM_ITEMTYPE_ANGLE, runtime);
		break;

	case DOM_Runtime::SVGTRANSFORM_PROTOTYPE:
		DOM_SVGObject::ConstructDOMImplementationSVGObjectL(*object, SVG_DOM_ITEMTYPE_TRANSFORM, runtime);
		break;

	case DOM_Runtime::SVGPAINT_PROTOTYPE:
		DOM_SVGObject::ConstructDOMImplementationSVGObjectL(*object, SVG_DOM_ITEMTYPE_PAINT, runtime);
		break;

	case DOM_Runtime::SVGASPECTRATIO_PROTOTYPE:
		DOM_SVGObject::ConstructDOMImplementationSVGObjectL(*object, SVG_DOM_ITEMTYPE_PRESERVE_ASPECT_RATIO, runtime);
		break;

	case DOM_Runtime::SVGPATHSEG_PROTOTYPE:
		DOM_SVGObject::ConstructDOMImplementationSVGObjectL(*object, SVG_DOM_ITEMTYPE_PATHSEG, runtime);
		break;
#endif // SVG_FULL_11

#ifdef SVG_TINY_12
	case DOM_Runtime::SVGRGBCOLOR_PROTOTYPE:
		DOM_SVGObject::ConstructDOMImplementationSVGObjectL(*object, SVG_DOM_ITEMTYPE_RGBCOLOR, runtime);
		break;

	case DOM_Runtime::SVGPATH_PROTOTYPE:
		DOM_SVGObject::ConstructDOMImplementationSVGObjectL(*object, SVG_DOM_ITEMTYPE_PATH, runtime);
		break;
#endif // SVG_TINY_12
	}

	return *object;
}

/* static */ void
DOM_SVGObject::ConstructDOMImplementationSVGObjectL(ES_Object *obj, SVGDOMItemType type, DOM_Runtime *runtime)
{
#ifdef SVG_FULL_11
	if (type == SVG_DOM_ITEMTYPE_LENGTH)
	{
		DOM_Object::PutNumericConstantL(obj, "SVG_LENGTHTYPE_UNKNOWN", SVGDOMLength::SVG_DOM_LENGTHTYPE_UNKNOWN, runtime);
		DOM_Object::PutNumericConstantL(obj, "SVG_LENGTHTYPE_NUMBER", SVGDOMLength::SVG_DOM_LENGTHTYPE_NUMBER, runtime);
		DOM_Object::PutNumericConstantL(obj, "SVG_LENGTHTYPE_PERCENTAGE", SVGDOMLength::SVG_DOM_LENGTHTYPE_PERCENTAGE, runtime);
		DOM_Object::PutNumericConstantL(obj, "SVG_LENGTHTYPE_EMS", SVGDOMLength::SVG_DOM_LENGTHTYPE_EMS, runtime);
		DOM_Object::PutNumericConstantL(obj, "SVG_LENGTHTYPE_EXS", SVGDOMLength::SVG_DOM_LENGTHTYPE_EXS, runtime);
		DOM_Object::PutNumericConstantL(obj, "SVG_LENGTHTYPE_PX", SVGDOMLength::SVG_DOM_LENGTHTYPE_PX, runtime);
		DOM_Object::PutNumericConstantL(obj, "SVG_LENGTHTYPE_CM", SVGDOMLength::SVG_DOM_LENGTHTYPE_CM, runtime);
		DOM_Object::PutNumericConstantL(obj, "SVG_LENGTHTYPE_MM", SVGDOMLength::SVG_DOM_LENGTHTYPE_MM, runtime);
		DOM_Object::PutNumericConstantL(obj, "SVG_LENGTHTYPE_IN", SVGDOMLength::SVG_DOM_LENGTHTYPE_IN, runtime);
		DOM_Object::PutNumericConstantL(obj, "SVG_LENGTHTYPE_PT", SVGDOMLength::SVG_DOM_LENGTHTYPE_PT, runtime);
		DOM_Object::PutNumericConstantL(obj, "SVG_LENGTHTYPE_PC", SVGDOMLength::SVG_DOM_LENGTHTYPE_PC, runtime);
		DOM_Object::PutNumericConstantL(obj, "SVG_LENGTHTYPE_REMS", SVGDOMLength::SVG_DOM_LENGTHTYPE_REMS, runtime);
	}

	if (type == SVG_DOM_ITEMTYPE_ANGLE)
	{
		DOM_Object::PutNumericConstantL(obj, "SVG_ANGLETYPE_UNKNOWN", SVGDOMAngle::SVG_ANGLETYPE_UNKNOWN, runtime);
		DOM_Object::PutNumericConstantL(obj, "SVG_ANGLETYPE_UNSPECIFIED", SVGDOMAngle::SVG_ANGLETYPE_UNSPECIFIED, runtime);
		DOM_Object::PutNumericConstantL(obj, "SVG_ANGLETYPE_DEG", SVGDOMAngle::SVG_ANGLETYPE_DEG, runtime);
		DOM_Object::PutNumericConstantL(obj, "SVG_ANGLETYPE_RAD", SVGDOMAngle::SVG_ANGLETYPE_RAD, runtime);
		DOM_Object::PutNumericConstantL(obj, "SVG_ANGLETYPE_GRAD", SVGDOMAngle::SVG_ANGLETYPE_GRAD, runtime);
	}

	if (type == SVG_DOM_ITEMTYPE_TRANSFORM)
	{
		DOM_Object::PutNumericConstantL(obj, "SVG_TRANSFORM_UNKNOWN", SVGDOMTransform::SVG_TRANSFORM_UNKNOWN, runtime);
		DOM_Object::PutNumericConstantL(obj, "SVG_TRANSFORM_MATRIX", SVGDOMTransform::SVG_TRANSFORM_MATRIX, runtime);
		DOM_Object::PutNumericConstantL(obj, "SVG_TRANSFORM_TRANSLATE", SVGDOMTransform::SVG_TRANSFORM_TRANSLATE, runtime);
		DOM_Object::PutNumericConstantL(obj, "SVG_TRANSFORM_SCALE", SVGDOMTransform::SVG_TRANSFORM_SCALE, runtime);
		DOM_Object::PutNumericConstantL(obj, "SVG_TRANSFORM_ROTATE", SVGDOMTransform::SVG_TRANSFORM_ROTATE, runtime);
		DOM_Object::PutNumericConstantL(obj, "SVG_TRANSFORM_SKEWX", SVGDOMTransform::SVG_TRANSFORM_SKEWX, runtime);
		DOM_Object::PutNumericConstantL(obj, "SVG_TRANSFORM_SKEWY", SVGDOMTransform::SVG_TRANSFORM_SKEWY, runtime);
	}

	if (type == SVG_DOM_ITEMTYPE_PAINT)
	{
		DOM_Object::PutNumericConstantL(obj, "SVG_PAINTTYPE_UNKNOWN", SVGDOMPaint::SVG_PAINTTYPE_UNKNOWN, runtime);
		DOM_Object::PutNumericConstantL(obj, "SVG_PAINTTYPE_RGBCOLOR", SVGDOMPaint::SVG_PAINTTYPE_RGBCOLOR, runtime);
		DOM_Object::PutNumericConstantL(obj, "SVG_PAINTTYPE_RGBCOLOR_ICCCOLOR", SVGDOMPaint::SVG_PAINTTYPE_RGBCOLOR_ICCCOLOR, runtime);
		DOM_Object::PutNumericConstantL(obj, "SVG_PAINTTYPE_NONE", SVGDOMPaint::SVG_PAINTTYPE_NONE, runtime);
		DOM_Object::PutNumericConstantL(obj, "SVG_PAINTTYPE_CURRENTCOLOR", SVGDOMPaint::SVG_PAINTTYPE_CURRENTCOLOR, runtime);
		DOM_Object::PutNumericConstantL(obj, "SVG_PAINTTYPE_URI_NONE", SVGDOMPaint::SVG_PAINTTYPE_URI_NONE, runtime);
		DOM_Object::PutNumericConstantL(obj, "SVG_PAINTTYPE_URI_CURRENTCOLOR", SVGDOMPaint::SVG_PAINTTYPE_URI_CURRENTCOLOR, runtime);
		DOM_Object::PutNumericConstantL(obj, "SVG_PAINTTYPE_URI_RGBCOLOR", SVGDOMPaint::SVG_PAINTTYPE_URI_RGBCOLOR, runtime);
		DOM_Object::PutNumericConstantL(obj, "SVG_PAINTTYPE_URI_RGBCOLOR_ICCCOLOR", SVGDOMPaint::SVG_PAINTTYPE_URI_RGBCOLOR_ICCCOLOR, runtime);
		DOM_Object::PutNumericConstantL(obj, "SVG_PAINTTYPE_URI", SVGDOMPaint::SVG_PAINTTYPE_URI, runtime);
	}

	if (type == SVG_DOM_ITEMTYPE_COLOR)
	{
		DOM_Object::PutNumericConstantL(obj, "SVG_COLORTYPE_UNKNOWN", SVGDOMColor::SVG_COLORTYPE_UNKNOWN, runtime);
		DOM_Object::PutNumericConstantL(obj, "SVG_COLORTYPE_RGBCOLOR", SVGDOMColor::SVG_COLORTYPE_RGBCOLOR, runtime);
		DOM_Object::PutNumericConstantL(obj, "SVG_COLORTYPE_RGBCOLOR_ICCCOLOR", SVGDOMColor::SVG_COLORTYPE_RGBCOLOR_ICCCOLOR, runtime);
		DOM_Object::PutNumericConstantL(obj, "SVG_COLORTYPE_CURRENTCOLOR", SVGDOMColor::SVG_COLORTYPE_CURRENTCOLOR, runtime);
	}

	if (type == SVG_DOM_ITEMTYPE_PRESERVE_ASPECT_RATIO)
	{
		DOM_Object::PutNumericConstantL(obj, "SVG_PRESERVEASPECTRATIO_UNKNOWN", SVGDOMPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_UNKNOWN, runtime);
		DOM_Object::PutNumericConstantL(obj, "SVG_PRESERVEASPECTRATIO_NONE", SVGDOMPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_NONE, runtime);
		DOM_Object::PutNumericConstantL(obj, "SVG_PRESERVEASPECTRATIO_XMINYMIN", SVGDOMPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMINYMIN, runtime);
		DOM_Object::PutNumericConstantL(obj, "SVG_PRESERVEASPECTRATIO_XMIDYMIN", SVGDOMPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMIN, runtime);
		DOM_Object::PutNumericConstantL(obj, "SVG_PRESERVEASPECTRATIO_XMAXYMIN", SVGDOMPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMAXYMIN, runtime);
		DOM_Object::PutNumericConstantL(obj, "SVG_PRESERVEASPECTRATIO_XMINYMID", SVGDOMPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMINYMID, runtime);
		DOM_Object::PutNumericConstantL(obj, "SVG_PRESERVEASPECTRATIO_XMIDYMID", SVGDOMPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMID, runtime);
		DOM_Object::PutNumericConstantL(obj, "SVG_PRESERVEASPECTRATIO_XMAXYMID", SVGDOMPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMAXYMID, runtime);
		DOM_Object::PutNumericConstantL(obj, "SVG_PRESERVEASPECTRATIO_XMINYMAX", SVGDOMPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMINYMAX, runtime);
		DOM_Object::PutNumericConstantL(obj, "SVG_PRESERVEASPECTRATIO_XMIDYMAX", SVGDOMPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMAX, runtime);
		DOM_Object::PutNumericConstantL(obj, "SVG_PRESERVEASPECTRATIO_XMAXYMAX", SVGDOMPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMAXYMAX, runtime);

		DOM_Object::PutNumericConstantL(obj, "SVG_MEETORSLICE_UNKNOWN", SVGDOMPreserveAspectRatio::SVG_MEETORSLICE_UNKNOWN, runtime);
		DOM_Object::PutNumericConstantL(obj, "SVG_MEETORSLICE_MEET", SVGDOMPreserveAspectRatio::SVG_MEETORSLICE_MEET, runtime);
		DOM_Object::PutNumericConstantL(obj, "SVG_MEETORSLICE_SLICE", SVGDOMPreserveAspectRatio::SVG_MEETORSLICE_SLICE, runtime);
	}

	if (type == SVG_DOM_ITEMTYPE_PATHSEG)
	{
		DOM_Object::PutNumericConstantL(obj, "PATHSEG_UNKNOWN", SVGDOMPathSeg::SVG_DOM_PATHSEG_UNKNOWN, runtime);
		DOM_Object::PutNumericConstantL(obj, "PATHSEG_CLOSEPATH", SVGDOMPathSeg::SVG_DOM_PATHSEG_CLOSEPATH, runtime);
		DOM_Object::PutNumericConstantL(obj, "PATHSEG_MOVETO_ABS", SVGDOMPathSeg::SVG_DOM_PATHSEG_MOVETO_ABS, runtime);
		DOM_Object::PutNumericConstantL(obj, "PATHSEG_MOVETO_REL", SVGDOMPathSeg::SVG_DOM_PATHSEG_MOVETO_REL, runtime);
		DOM_Object::PutNumericConstantL(obj, "PATHSEG_LINETO_ABS", SVGDOMPathSeg::SVG_DOM_PATHSEG_LINETO_ABS, runtime);
		DOM_Object::PutNumericConstantL(obj, "PATHSEG_LINETO_REL", SVGDOMPathSeg::SVG_DOM_PATHSEG_LINETO_REL, runtime);
		DOM_Object::PutNumericConstantL(obj, "PATHSEG_CURVETO_CUBIC_ABS", SVGDOMPathSeg::SVG_DOM_PATHSEG_CURVETO_CUBIC_ABS, runtime);
		DOM_Object::PutNumericConstantL(obj, "PATHSEG_CURVETO_CUBIC_REL", SVGDOMPathSeg::SVG_DOM_PATHSEG_CURVETO_CUBIC_REL, runtime);
		DOM_Object::PutNumericConstantL(obj, "PATHSEG_CURVETO_QUADRATIC_ABS", SVGDOMPathSeg::SVG_DOM_PATHSEG_CURVETO_QUADRATIC_ABS, runtime);
		DOM_Object::PutNumericConstantL(obj, "PATHSEG_CURVETO_QUADRATIC_REL", SVGDOMPathSeg::SVG_DOM_PATHSEG_CURVETO_QUADRATIC_REL, runtime);
		DOM_Object::PutNumericConstantL(obj, "PATHSEG_ARC_ABS", SVGDOMPathSeg::SVG_DOM_PATHSEG_ARC_ABS, runtime);
		DOM_Object::PutNumericConstantL(obj, "PATHSEG_ARC_REL", SVGDOMPathSeg::SVG_DOM_PATHSEG_ARC_REL, runtime);
		DOM_Object::PutNumericConstantL(obj, "PATHSEG_LINETO_HORIZONTAL_ABS", SVGDOMPathSeg::SVG_DOM_PATHSEG_LINETO_HORIZONTAL_ABS, runtime);
		DOM_Object::PutNumericConstantL(obj, "PATHSEG_LINETO_HORIZONTAL_REL", SVGDOMPathSeg::SVG_DOM_PATHSEG_LINETO_HORIZONTAL_REL, runtime);
		DOM_Object::PutNumericConstantL(obj, "PATHSEG_LINETO_VERTICAL_ABS", SVGDOMPathSeg::SVG_DOM_PATHSEG_LINETO_VERTICAL_ABS, runtime);
		DOM_Object::PutNumericConstantL(obj, "PATHSEG_LINETO_VERTICAL_REL", SVGDOMPathSeg::SVG_DOM_PATHSEG_LINETO_VERTICAL_REL, runtime);
		DOM_Object::PutNumericConstantL(obj, "PATHSEG_CURVETO_CUBIC_SMOOTH_ABS", SVGDOMPathSeg::SVG_DOM_PATHSEG_CURVETO_CUBIC_SMOOTH_ABS, runtime);
		DOM_Object::PutNumericConstantL(obj, "PATHSEG_CURVETO_CUBIC_SMOOTH_REL", SVGDOMPathSeg::SVG_DOM_PATHSEG_CURVETO_CUBIC_SMOOTH_REL, runtime);
		DOM_Object::PutNumericConstantL(obj, "PATHSEG_CURVETO_QUADRATIC_SMOOTH_ABS", SVGDOMPathSeg::SVG_DOM_PATHSEG_CURVETO_QUADRATIC_SMOOTH_ABS, runtime);
		DOM_Object::PutNumericConstantL(obj, "PATHSEG_CURVETO_QUADRATIC_SMOOTH_REL", SVGDOMPathSeg::SVG_DOM_PATHSEG_CURVETO_QUADRATIC_SMOOTH_REL, runtime);
	}

	if (type == SVG_DOM_ITEMTYPE_CSSPRIMITIVEVALUE)
	{
		DOM_Object::PutNumericConstantL(obj, "CSS_UNKNOWN", SVGDOMCSSPrimitiveValue::SVG_DOM_CSS_UNKNOWN, runtime);
		DOM_Object::PutNumericConstantL(obj, "CSS_NUMBER", SVGDOMCSSPrimitiveValue::SVG_DOM_CSS_NUMBER, runtime);
		DOM_Object::PutNumericConstantL(obj, "CSS_PERCENTAGE", SVGDOMCSSPrimitiveValue::SVG_DOM_CSS_PERCENTAGE, runtime);
		DOM_Object::PutNumericConstantL(obj, "CSS_EMS", SVGDOMCSSPrimitiveValue::SVG_DOM_CSS_EMS, runtime);
		DOM_Object::PutNumericConstantL(obj, "CSS_EXS", SVGDOMCSSPrimitiveValue::SVG_DOM_CSS_EXS, runtime);
		DOM_Object::PutNumericConstantL(obj, "CSS_PX", SVGDOMCSSPrimitiveValue::SVG_DOM_CSS_PX, runtime);
		DOM_Object::PutNumericConstantL(obj, "CSS_CM", SVGDOMCSSPrimitiveValue::SVG_DOM_CSS_CM, runtime);
		DOM_Object::PutNumericConstantL(obj, "CSS_MM", SVGDOMCSSPrimitiveValue::SVG_DOM_CSS_MM, runtime);
		DOM_Object::PutNumericConstantL(obj, "CSS_IN", SVGDOMCSSPrimitiveValue::SVG_DOM_CSS_IN, runtime);
		DOM_Object::PutNumericConstantL(obj, "CSS_PT", SVGDOMCSSPrimitiveValue::SVG_DOM_CSS_PT, runtime);
		DOM_Object::PutNumericConstantL(obj, "CSS_PC", SVGDOMCSSPrimitiveValue::SVG_DOM_CSS_PC, runtime);
		DOM_Object::PutNumericConstantL(obj, "CSS_DEG", SVGDOMCSSPrimitiveValue::SVG_DOM_CSS_DEG, runtime);
		DOM_Object::PutNumericConstantL(obj, "CSS_RAD", SVGDOMCSSPrimitiveValue::SVG_DOM_CSS_RAD, runtime);
		DOM_Object::PutNumericConstantL(obj, "CSS_GRAD", SVGDOMCSSPrimitiveValue::SVG_DOM_CSS_GRAD, runtime);
		DOM_Object::PutNumericConstantL(obj, "CSS_MS", SVGDOMCSSPrimitiveValue::SVG_DOM_CSS_MS, runtime);
		DOM_Object::PutNumericConstantL(obj, "CSS_S", SVGDOMCSSPrimitiveValue::SVG_DOM_CSS_S, runtime);
		DOM_Object::PutNumericConstantL(obj, "CSS_HZ", SVGDOMCSSPrimitiveValue::SVG_DOM_CSS_HZ, runtime);
		DOM_Object::PutNumericConstantL(obj, "CSS_KHZ", SVGDOMCSSPrimitiveValue::SVG_DOM_CSS_KHZ, runtime);
		DOM_Object::PutNumericConstantL(obj, "CSS_DIMENSION", SVGDOMCSSPrimitiveValue::SVG_DOM_CSS_DIMENSION, runtime);
		DOM_Object::PutNumericConstantL(obj, "CSS_STRING", SVGDOMCSSPrimitiveValue::SVG_DOM_CSS_STRING, runtime);
		DOM_Object::PutNumericConstantL(obj, "CSS_URI", SVGDOMCSSPrimitiveValue::SVG_DOM_CSS_URI, runtime);
		DOM_Object::PutNumericConstantL(obj, "CSS_IDENT", SVGDOMCSSPrimitiveValue::SVG_DOM_CSS_IDENT, runtime);
		DOM_Object::PutNumericConstantL(obj, "CSS_ATTR", SVGDOMCSSPrimitiveValue::SVG_DOM_CSS_ATTR, runtime);
		DOM_Object::PutNumericConstantL(obj, "CSS_COUNTER", SVGDOMCSSPrimitiveValue::SVG_DOM_CSS_COUNTER, runtime);
		DOM_Object::PutNumericConstantL(obj, "CSS_RECT", SVGDOMCSSPrimitiveValue::SVG_DOM_CSS_RECT, runtime);
		DOM_Object::PutNumericConstantL(obj, "CSS_RGBCOLOR", SVGDOMCSSPrimitiveValue::SVG_DOM_CSS_RGBCOLOR, runtime);
	}
#endif // SVG_FULL_11
#ifdef SVG_TINY_12
	if (type == SVG_DOM_ITEMTYPE_PATH)
	{
		DOM_Object::PutNumericConstantL(obj, "MOVE_TO", SVGDOMPath::SVGPATH_MOVETO, runtime);
		DOM_Object::PutNumericConstantL(obj, "LINE_TO", SVGDOMPath::SVGPATH_LINETO, runtime);
		DOM_Object::PutNumericConstantL(obj, "CURVE_TO", SVGDOMPath::SVGPATH_CURVETO, runtime);
		DOM_Object::PutNumericConstantL(obj, "QUAD_TO", SVGDOMPath::SVGPATH_QUADTO, runtime);
		DOM_Object::PutNumericConstantL(obj, "CLOSE", SVGDOMPath::SVGPATH_CLOSE, runtime);
	}
#endif // SVG_TINY_12
}

/* virtual */ ES_GetState
DOM_SVGObject::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
#ifdef SVG_FULL_11
	if (svg_item->IsA(SVG_DOM_ITEMTYPE_NUMBER))
	{
		SVGDOMNumber* svg_number = static_cast<SVGDOMNumber*>(svg_item);
		switch(property_name)
		{
		case OP_ATOM_value:
		{
			if (value)
			{
				double number = svg_number->GetValue();
				DOMSetNumber(value, number);
			}
			return GET_SUCCESS;
		}
		}
	}

	if (svg_item->IsA(SVG_DOM_ITEMTYPE_LENGTH))
	{
		SVGDOMLength* svg_length = static_cast<SVGDOMLength*>(svg_item);
		switch(property_name)
		{
		case OP_ATOM_unitType:
		{
			if (value)
			{
				UINT32 unit_type = svg_length->GetUnitType();
				DOMSetNumber(value, unit_type);
			}
			return GET_SUCCESS;
		}
		case OP_ATOM_value:
		{
			return GetLengthValue(property_name, value, svg_length, origining_runtime);
		}
		case OP_ATOM_valueInSpecifiedUnits:
		{
			if (value)
			{
				double len_value = svg_length->GetValueInSpecifiedUnits();
				DOMSetNumber(value, len_value);
			}
			return GET_SUCCESS;
		}
		case OP_ATOM_valueAsString:
		{
			if (value)
			{
				const uni_char* len_value = svg_length->GetValueAsString();
				DOMSetString(value, len_value);
			}
			return GET_SUCCESS;
		}
		default:
			break;
		}
	}

	if (svg_item->IsA(SVG_DOM_ITEMTYPE_ANGLE))
	{
		SVGDOMAngle* svg_angle = static_cast<SVGDOMAngle*>(svg_item);
		switch (property_name)
		{
		case OP_ATOM_unitType:
		{
			if (value)
			{
				UINT32 unit_type = svg_angle->GetUnitType();
				DOMSetNumber(value, unit_type);
			}
			return GET_SUCCESS;
		}
		case OP_ATOM_value:
		{
			if (value)
			{
				double angle_value = svg_angle->GetValue();
				DOMSetNumber(value, angle_value);
			}
			return GET_SUCCESS;
		}
		case OP_ATOM_valueInSpecifiedUnits:
		{
			if (value)
			{
				double angle_value = svg_angle->GetValueInSpecifiedUnits();
				DOMSetNumber(value, angle_value);
			}
			return GET_SUCCESS;
		}
		case OP_ATOM_valueAsString:
		{
			if (value)
			{
				const uni_char* angle_value = svg_angle->GetValueAsString();
				DOMSetString(value, angle_value);
			}
			return GET_SUCCESS;
		}
		default:
			break;
		}
	}

	if (svg_item->IsA(SVG_DOM_ITEMTYPE_PATHSEG))
	{
		SVGDOMPathSeg* svg_pathseg = static_cast<SVGDOMPathSeg*>(svg_item);
		switch (property_name)
		{
		case OP_ATOM_pathSegType:
			if (value)
			{
				DOMSetNumber(value, svg_pathseg->GetSegType());
			}
			return GET_SUCCESS;
		case OP_ATOM_pathSegTypeAsLetter:
		{
			if (value)
			{
				TempBuffer* buffer = GetEmptyTempBuf();
				if (OpStatus::IsSuccess(buffer->Append(svg_pathseg->GetSegTypeAsLetter())))
					DOMSetString(value, buffer);
			}
			return GET_SUCCESS;
		}
		break;

		default:
		{
			SVGDOMPathSeg::PathSegType pstype = (SVGDOMPathSeg::PathSegType)svg_pathseg->GetSegType();

			if (pstype == SVGDOMPathSeg::SVG_DOM_PATHSEG_UNKNOWN ||
				pstype == SVGDOMPathSeg::SVG_DOM_PATHSEG_CLOSEPATH)
				break;

			switch (property_name)
			{
			case OP_ATOM_x: // !vert{rel,abs}
				if (pstype != SVGDOMPathSeg::SVG_DOM_PATHSEG_LINETO_VERTICAL_ABS &&
					pstype != SVGDOMPathSeg::SVG_DOM_PATHSEG_LINETO_VERTICAL_REL)
				{
					if (value)
						DOMSetNumber(value, svg_pathseg->GetX());
					return GET_SUCCESS;
				}
				break;
			case OP_ATOM_y: // !hor{rel,abs}
				if (pstype != SVGDOMPathSeg::SVG_DOM_PATHSEG_LINETO_HORIZONTAL_ABS &&
					pstype != SVGDOMPathSeg::SVG_DOM_PATHSEG_LINETO_HORIZONTAL_REL)
				{
					if (value)
						DOMSetNumber(value, svg_pathseg->GetY());
					return GET_SUCCESS;
				}
				break;
			case OP_ATOM_x1: // cubic{r,a} && quadratic{r,a}
				if (pstype == SVGDOMPathSeg::SVG_DOM_PATHSEG_CURVETO_CUBIC_ABS ||
					pstype == SVGDOMPathSeg::SVG_DOM_PATHSEG_CURVETO_CUBIC_REL ||
					pstype == SVGDOMPathSeg::SVG_DOM_PATHSEG_CURVETO_QUADRATIC_ABS ||
					pstype == SVGDOMPathSeg::SVG_DOM_PATHSEG_CURVETO_QUADRATIC_REL)
				{
					if (value)
						DOMSetNumber(value, svg_pathseg->GetX1());
					return GET_SUCCESS;
				}
				break;
			case OP_ATOM_y1: // cubic{r,a} && quadratic{r,a}
				if (pstype == SVGDOMPathSeg::SVG_DOM_PATHSEG_CURVETO_CUBIC_ABS ||
					pstype == SVGDOMPathSeg::SVG_DOM_PATHSEG_CURVETO_CUBIC_REL ||
					pstype == SVGDOMPathSeg::SVG_DOM_PATHSEG_CURVETO_QUADRATIC_ABS ||
					pstype == SVGDOMPathSeg::SVG_DOM_PATHSEG_CURVETO_QUADRATIC_REL)
				{
					if (value)
						DOMSetNumber(value, svg_pathseg->GetY1());
					return GET_SUCCESS;
				}
				break;
			case OP_ATOM_x2: // cubic*
				if (pstype == SVGDOMPathSeg::SVG_DOM_PATHSEG_CURVETO_CUBIC_ABS ||
					pstype == SVGDOMPathSeg::SVG_DOM_PATHSEG_CURVETO_CUBIC_REL ||
					pstype == SVGDOMPathSeg::SVG_DOM_PATHSEG_CURVETO_CUBIC_SMOOTH_ABS ||
					pstype == SVGDOMPathSeg::SVG_DOM_PATHSEG_CURVETO_CUBIC_SMOOTH_REL)
				{
					if (value)
						DOMSetNumber(value, svg_pathseg->GetX2());
					return GET_SUCCESS;
				}
				break;
			case OP_ATOM_y2: // cubic*
				if (pstype == SVGDOMPathSeg::SVG_DOM_PATHSEG_CURVETO_CUBIC_ABS ||
					pstype == SVGDOMPathSeg::SVG_DOM_PATHSEG_CURVETO_CUBIC_REL ||
					pstype == SVGDOMPathSeg::SVG_DOM_PATHSEG_CURVETO_CUBIC_SMOOTH_ABS ||
					pstype == SVGDOMPathSeg::SVG_DOM_PATHSEG_CURVETO_CUBIC_SMOOTH_REL)
				{
					if (value)
						DOMSetNumber(value, svg_pathseg->GetY2());
					return GET_SUCCESS;
				}
				break;
			case OP_ATOM_r1: // arc{r,a}
				if (pstype == SVGDOMPathSeg::SVG_DOM_PATHSEG_ARC_ABS ||
					pstype == SVGDOMPathSeg::SVG_DOM_PATHSEG_ARC_REL)
				{
					if (value)
						DOMSetNumber(value, svg_pathseg->GetR1());
					return GET_SUCCESS;
				}
				break;
			case OP_ATOM_r2: // arc{r,a}
				if (pstype == SVGDOMPathSeg::SVG_DOM_PATHSEG_ARC_ABS ||
					pstype == SVGDOMPathSeg::SVG_DOM_PATHSEG_ARC_REL)
				{
					if (value)
						DOMSetNumber(value, svg_pathseg->GetR2());
					return GET_SUCCESS;
				}
				break;
			case OP_ATOM_angle: // arc{r,a}
				if (pstype == SVGDOMPathSeg::SVG_DOM_PATHSEG_ARC_ABS ||
					pstype == SVGDOMPathSeg::SVG_DOM_PATHSEG_ARC_REL)
				{
					if (value)
						DOMSetNumber(value, svg_pathseg->GetAngle());
					return GET_SUCCESS;
				}
				break;
			case OP_ATOM_largeArcFlag: // arc{r,a}
				if (pstype == SVGDOMPathSeg::SVG_DOM_PATHSEG_ARC_ABS ||
					pstype == SVGDOMPathSeg::SVG_DOM_PATHSEG_ARC_REL)
				{
					if (value)
						DOMSetBoolean(value, svg_pathseg->GetLargeArcFlag());
					return GET_SUCCESS;
				}
				break;
			case OP_ATOM_sweepFlag: // arc{r,a}
				if (pstype == SVGDOMPathSeg::SVG_DOM_PATHSEG_ARC_ABS ||
					pstype == SVGDOMPathSeg::SVG_DOM_PATHSEG_ARC_REL)
				{
					if (value)
						DOMSetBoolean(value, svg_pathseg->GetSweepFlag());
					return GET_SUCCESS;
				}
				break;
			default:
				break;
			}
		}
		break;
		}
	}
#endif // SVG_FULL_11

	if (svg_item->IsA(SVG_DOM_ITEMTYPE_POINT))
	{
		SVGDOMPoint* svg_point = static_cast<SVGDOMPoint*>(svg_item);
		switch(property_name)
		{
		case OP_ATOM_x:
			if (value)
				DOMSetNumber(value, svg_point->GetX());
			return GET_SUCCESS;
		case OP_ATOM_y:
			if (value)
				DOMSetNumber(value, svg_point->GetY());
			return GET_SUCCESS;
		default:
			break;
		}
	}

#ifdef SVG_FULL_11
	if (svg_item->IsA(SVG_DOM_ITEMTYPE_PRESERVE_ASPECT_RATIO))
	{
		SVGDOMPreserveAspectRatio* svg_par = static_cast<SVGDOMPreserveAspectRatio*>(svg_item);
		switch (property_name)
		{
		case OP_ATOM_align:
			if (value)
				DOMSetNumber(value, svg_par->GetAlign());
			return GET_SUCCESS;
		case OP_ATOM_meetOrSlice:
			if (value)
				DOMSetNumber(value, svg_par->GetMeetOrSlice());
			return GET_SUCCESS;
		default:
			break;
		}
	}

	// Note that SVG_TINY_12 has a getComponent method instead of the following
	if (svg_item->IsA(SVG_DOM_ITEMTYPE_MATRIX))
	{
		SVGDOMMatrix* svg_matrix = static_cast<SVGDOMMatrix*>(svg_item);
		switch(property_name)
		{
		case OP_ATOM_a:
		{
			if (value)
				DOMSetNumber(value, svg_matrix->GetValue(0));
			return GET_SUCCESS;
		}
		case OP_ATOM_b:
		{
			if (value)
				DOMSetNumber(value, svg_matrix->GetValue(1));
			return GET_SUCCESS;
		}
		case OP_ATOM_c:
		{
			if (value)
				DOMSetNumber(value, svg_matrix->GetValue(2));
			return GET_SUCCESS;
		}
		case OP_ATOM_d:
		{
			if (value)
				DOMSetNumber(value, svg_matrix->GetValue(3));
			return GET_SUCCESS;
		}
		case OP_ATOM_e:
		{
			if (value)
				DOMSetNumber(value, svg_matrix->GetValue(4));
			return GET_SUCCESS;
		}
		case OP_ATOM_f:
		{
			if (value)
				DOMSetNumber(value, svg_matrix->GetValue(5));
			return GET_SUCCESS;
		}
		default:
			break;
		}
	}
#endif // SVG_FULL_11

	if (svg_item->IsA(SVG_DOM_ITEMTYPE_RECT))
	{
		SVGDOMRect* svg_rect = static_cast<SVGDOMRect*>(svg_item);
		switch(property_name)
		{
		case OP_ATOM_x:
			if (value)
				DOMSetNumber(value, svg_rect->GetX());
			return GET_SUCCESS;
		case OP_ATOM_y:
			if (value)
				DOMSetNumber(value, svg_rect->GetY());
			return GET_SUCCESS;
		case OP_ATOM_width:
			if (value)
				DOMSetNumber(value, svg_rect->GetWidth());
			return GET_SUCCESS;
		case OP_ATOM_height:
			if (value)
				DOMSetNumber(value, svg_rect->GetHeight());
			return GET_SUCCESS;
		default:
			break;
		}
	}

#ifdef SVG_FULL_11
	if (svg_item->IsA(SVG_DOM_ITEMTYPE_TRANSFORM))
	{
		SVGDOMTransform* svg_transform = static_cast<SVGDOMTransform*>(svg_item);
		switch(property_name)
		{
		case OP_ATOM_type:
		{
			if (value)
			{
				SVGDOMTransform::TransformType type = svg_transform->GetType();
				DOMSetNumber(value, (double)type);
			}
			return GET_SUCCESS;
		}
		break;
		case OP_ATOM_angle:
		{
			if (value)
			{
				double angle = svg_transform->GetAngle();
				DOMSetNumber(value, angle);
			}
			return GET_SUCCESS;
		}
		break;
		case OP_ATOM_matrix:
		{
			if (value)
			{
				DOM_SVGObject* matrix = (DOM_SVGObject*)object_store->GetObject(OP_ATOM_matrix);
				if (!DOM_SVGLocation::IsValid(matrix))
				{
					SVGDOMMatrix* svg_matrix;
					if (OpStatus::IsMemoryError(svg_transform->GetMatrix(svg_matrix)))
						return GET_NO_MEMORY;

					if (OpStatus::IsMemoryError(DOM_SVGObject::Make(matrix, svg_matrix, location, GetEnvironment())))
					{
						OP_DELETE(svg_matrix);
						return GET_NO_MEMORY;
					}
					object_store->SetObject(OP_ATOM_matrix, matrix);
				}

				DOMSetObject(value, matrix);
			}
			return GET_SUCCESS;
		}
		break;
		default:
			break;
		}
	}

	if (svg_item->IsA(SVG_DOM_ITEMTYPE_PAINT))
	{
		SVGDOMPaint* svg_paint = static_cast<SVGDOMPaint*>(svg_item);
		switch(property_name)
		{
		case OP_ATOM_paintType:
		{
			if (value)
			{
				SVGDOMPaint::PaintType paint_type = svg_paint->GetPaintType();
				double paint_type_number_rep = (double)paint_type;
				DOMSetNumber(value, paint_type_number_rep);
			}
			return GET_SUCCESS;
		}
		break;
		case OP_ATOM_uri:
		{
			if (value)
			{
				const uni_char* uri_str = svg_paint->GetURI();
				if (uri_str != NULL)
					DOMSetString(value, uri_str);
				else
					DOMSetNull(value);
			}
			return GET_SUCCESS;
		}
		break;
		default:
			break;
		}
	}

	if (svg_item->IsA(SVG_DOM_ITEMTYPE_COLOR))
	{
		SVGDOMColor* svg_color = static_cast<SVGDOMColor*>(svg_item);
		switch(property_name)
		{
		case OP_ATOM_rgbColor:
		{
			if (value)
			{
				DOM_SVGObject* dom_obj = (DOM_SVGObject*)object_store->GetObject(OP_ATOM_rgbColor);
				if (!DOM_SVGLocation::IsValid(dom_obj))
				{
					SVGDOMCSSRGBColor* rgb_color;
					OP_STATUS status = svg_color->GetRGBColor(rgb_color);

					if (OpStatus::IsMemoryError(status))
						return GET_NO_MEMORY;

					if (OpStatus::IsMemoryError(DOM_SVGObject::Make(dom_obj, rgb_color, location, GetEnvironment())))
					{
						OP_DELETE(rgb_color);
						return GET_NO_MEMORY;
					}
					object_store->SetObject(OP_ATOM_rgbColor, dom_obj);
				}
				DOMSetObject(value, dom_obj);
			}
			return GET_SUCCESS;
		}
		break;
		default:
			break;
		}
	}

	if (svg_item->IsA(SVG_DOM_ITEMTYPE_CSSRGBCOLOR))
	{
		SVGDOMCSSRGBColor* css_rgb_color = static_cast<SVGDOMCSSRGBColor*>(svg_item);
		switch(property_name)
		{
		case OP_ATOM_red:
		{
			if (value)
			{
				DOM_SVGObject* dom_obj = (DOM_SVGObject*)object_store->GetObject(OP_ATOM_red);
				if (!DOM_SVGLocation::IsValid(dom_obj))
				{
					SVGDOMCSSPrimitiveValue* primitive_value = css_rgb_color->GetRed();
					if (!primitive_value)
						return GET_NO_MEMORY;
					if (OpStatus::IsMemoryError(DOM_SVGObject::Make(dom_obj, primitive_value, location, GetEnvironment())))
					{
						OP_DELETE(primitive_value);
						return GET_NO_MEMORY;
					}
					object_store->SetObject(OP_ATOM_red, dom_obj);
				}
				DOMSetObject(value, dom_obj);
			}
			return GET_SUCCESS;
		}
		break;
		case OP_ATOM_green:
		{
			if (value)
			{
				DOM_SVGObject* dom_obj = (DOM_SVGObject*)object_store->GetObject(OP_ATOM_green);
				if (!DOM_SVGLocation::IsValid(dom_obj))
				{
					SVGDOMCSSPrimitiveValue* primitive_value = css_rgb_color->GetGreen();
					if (!primitive_value)
						return GET_NO_MEMORY;
					if (OpStatus::IsMemoryError(DOM_SVGObject::Make(dom_obj, primitive_value, location, GetEnvironment())))
					{
						OP_DELETE(primitive_value);
						return GET_NO_MEMORY;
					}
					object_store->SetObject(OP_ATOM_green, dom_obj);
				}
				DOMSetObject(value, dom_obj);
			}
			return GET_SUCCESS;
		}
		break;
		case OP_ATOM_blue:
		{
			if (value)
			{
				DOM_SVGObject* dom_obj = (DOM_SVGObject*)object_store->GetObject(OP_ATOM_blue);
				if (!DOM_SVGLocation::IsValid(dom_obj))
				{
					SVGDOMCSSPrimitiveValue* primitive_value = css_rgb_color->GetBlue();
					if (!primitive_value)
						return GET_NO_MEMORY;
					if (OpStatus::IsMemoryError(DOM_SVGObject::Make(dom_obj, primitive_value, location, GetEnvironment())))
					{
						OP_DELETE(primitive_value);
						return GET_NO_MEMORY;
					}
					object_store->SetObject(OP_ATOM_blue, dom_obj);
				}
				DOMSetObject(value, dom_obj);
			}
			return GET_SUCCESS;
		}
		break;
		default:
			break;
		}
	}
#endif // SVG_FULL_11
#ifdef SVG_TINY_12
	if (svg_item->IsA(SVG_DOM_ITEMTYPE_PATH))
	{
		SVGDOMPath* svgdompath = static_cast<SVGDOMPath*>(svg_item);
		if(property_name == OP_ATOM_numberOfSegments)
		{
			DOMSetNumber(value, svgdompath->GetCount());
			return GET_SUCCESS;
		}
	}

	if (svg_item->IsA(SVG_DOM_ITEMTYPE_RGBCOLOR))
	{
		SVGDOMRGBColor* rgb_color = static_cast<SVGDOMRGBColor*>(svg_item);
		double num = 0;
		OP_STATUS status = OpStatus::ERR_NOT_SUPPORTED;
		switch(property_name)
		{
			case OP_ATOM_red:
			{
				if (value)
					status = rgb_color->GetComponent(0, num);
			}
			break;
			case OP_ATOM_green:
			{
				if (value)
					status = rgb_color->GetComponent(1, num);
			}
			break;
			case OP_ATOM_blue:
			{
				if (value)
					status = rgb_color->GetComponent(2, num);
			}
			break;
		}

		if(OpStatus::IsSuccess(status))
		{
			DOMSetNumber(value, num);
			return GET_SUCCESS;
		}
		else
		{
			if(OpStatus::ERR_OUT_OF_RANGE == status)
			{
				DOM_GETNAME_DOMEXCEPTION(TYPE_MISMATCH_ERR);
			}
			else
			{
				DOM_GETNAME_DOMEXCEPTION(NOT_SUPPORTED_ERR);
			}
		}
	}
#endif // SVG_TINY_12

	return DOM_Object::GetName(property_name, value, origining_runtime);
}

#define SET_PROPERTY(OBJ, PROPFUNC, ARGS)\
	do {\
		OP_BOOLEAN _ret = OBJ->Set##PROPFUNC ARGS;\
		PUT_FAILED_IF_ERROR(_ret);\
		if (_ret == OpBoolean::IS_TRUE)\
			location.Invalidate();\
	} while(0)

#define SET_PROPERTY_THROWS(OBJ, PROPFUNC, ARGS)\
	do {\
		OP_BOOLEAN _ret = OBJ->Set##PROPFUNC ARGS;\
		PUT_FAILED_IF_ERROR(_ret);\
		if (_ret == OpBoolean::IS_TRUE)\
			location.Invalidate();\
		else\
			return DOM_PUTNAME_DOMEXCEPTION(SYNTAX_ERR);\
	} while(0)

/* virtual */ ES_PutState
DOM_SVGObject::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
#ifdef SVG_FULL_11
	if (svg_item->IsA(SVG_DOM_ITEMTYPE_NUMBER))
	{
		SVGDOMNumber* svg_number = (SVGDOMNumber*)svg_item;
		if (property_name == OP_ATOM_value)
		{
			if (value->type == VALUE_NUMBER)
			{
				SET_PROPERTY(svg_number, Value, (value->value.number));
				return PUT_SUCCESS;
			}
			return PUT_NEEDS_NUMBER;
		}
	}

	if (svg_item->IsA(SVG_DOM_ITEMTYPE_LENGTH))
	{
		SVGDOMLength* svg_length = (SVGDOMLength*)svg_item;
		switch (property_name)
		{
		case OP_ATOM_value:
		{
			if (value->type == VALUE_NUMBER)
			{
				SET_PROPERTY(svg_length, Value, (value->value.number));
				return PUT_SUCCESS;
			}
			return PUT_NEEDS_NUMBER;
		}
		case OP_ATOM_valueInSpecifiedUnits:
		{
			if (value->type == VALUE_NUMBER)
			{
				SET_PROPERTY(svg_length, ValueInSpecifiedUnits, (value->value.number));
				return PUT_SUCCESS;
			}
			return PUT_NEEDS_NUMBER;
		}
		case OP_ATOM_valueAsString:
		{
			if (value->type == VALUE_STRING)
			{
				SET_PROPERTY_THROWS(svg_length, ValueAsString, (value->value.string));
				return PUT_SUCCESS;
			}
			return PUT_NEEDS_STRING;
		}
		default:
			break;
		}
	}

	if (svg_item->IsA(SVG_DOM_ITEMTYPE_ANGLE))
	{
		SVGDOMAngle* svg_angle = (SVGDOMAngle*)svg_item;
		switch (property_name)
		{
		case OP_ATOM_value:
		{
			if (value->type == VALUE_NUMBER)
			{
				SET_PROPERTY(svg_angle, Value, (value->value.number));
				return PUT_SUCCESS;
			}
			return PUT_NEEDS_NUMBER;
		}
		case OP_ATOM_valueInSpecifiedUnits:
		{
			if (value->type == VALUE_NUMBER)
			{
				SET_PROPERTY(svg_angle, ValueInSpecifiedUnits, (value->value.number));
				return PUT_SUCCESS;
			}
			return PUT_NEEDS_NUMBER;
		}
		case OP_ATOM_valueAsString:
		{
			if (value->type == VALUE_STRING)
			{
				SET_PROPERTY_THROWS(svg_angle, ValueAsString, (value->value.string));
				return PUT_SUCCESS;
			}
			return PUT_NEEDS_STRING;
		}
		default:
			break;
		}
	}

	if (svg_item->IsA(SVG_DOM_ITEMTYPE_PATHSEG))
	{
		SVGDOMPathSeg* svg_pathseg = (SVGDOMPathSeg*)svg_item;
		switch (property_name)
		{
		case OP_ATOM_pathSegType:
		case OP_ATOM_pathSegTypeAsLetter:
			return PUT_READ_ONLY;

		default:
		{
			SVGDOMPathSeg::PathSegType pstype = (SVGDOMPathSeg::PathSegType)svg_pathseg->GetSegType();

			if (pstype == SVGDOMPathSeg::SVG_DOM_PATHSEG_UNKNOWN ||
				pstype == SVGDOMPathSeg::SVG_DOM_PATHSEG_CLOSEPATH)
				break;

			switch (property_name)
			{
			case OP_ATOM_x: // !vert{rel,abs}
				if (pstype != SVGDOMPathSeg::SVG_DOM_PATHSEG_LINETO_VERTICAL_ABS &&
					pstype != SVGDOMPathSeg::SVG_DOM_PATHSEG_LINETO_VERTICAL_REL)
				{
					if (value->type == VALUE_NUMBER)
					{
						SET_PROPERTY(svg_pathseg, X, (value->value.number));
						return PUT_SUCCESS;
					}
					return PUT_NEEDS_NUMBER;
				}
				break;
			case OP_ATOM_y: // !hor{rel,abs}
				if (pstype != SVGDOMPathSeg::SVG_DOM_PATHSEG_LINETO_HORIZONTAL_ABS &&
					pstype != SVGDOMPathSeg::SVG_DOM_PATHSEG_LINETO_HORIZONTAL_REL)
				{
					if (value->type == VALUE_NUMBER)
					{
						SET_PROPERTY(svg_pathseg, Y, (value->value.number));
						return PUT_SUCCESS;
					}
					return PUT_NEEDS_NUMBER;
				}
				break;
			case OP_ATOM_x1: // cubic{r,a} && quadratic{r,a}
				if (pstype == SVGDOMPathSeg::SVG_DOM_PATHSEG_CURVETO_CUBIC_ABS ||
					pstype == SVGDOMPathSeg::SVG_DOM_PATHSEG_CURVETO_CUBIC_REL ||
					pstype == SVGDOMPathSeg::SVG_DOM_PATHSEG_CURVETO_QUADRATIC_ABS ||
					pstype == SVGDOMPathSeg::SVG_DOM_PATHSEG_CURVETO_QUADRATIC_REL)
				{
					if (value->type == VALUE_NUMBER)
					{
						SET_PROPERTY(svg_pathseg, X1, (value->value.number));
						return PUT_SUCCESS;
					}
					return PUT_NEEDS_NUMBER;
				}
				break;
			case OP_ATOM_y1: // cubic{r,a} && quadratic{r,a}
				if (pstype == SVGDOMPathSeg::SVG_DOM_PATHSEG_CURVETO_CUBIC_ABS ||
					pstype == SVGDOMPathSeg::SVG_DOM_PATHSEG_CURVETO_CUBIC_REL ||
					pstype == SVGDOMPathSeg::SVG_DOM_PATHSEG_CURVETO_QUADRATIC_ABS ||
					pstype == SVGDOMPathSeg::SVG_DOM_PATHSEG_CURVETO_QUADRATIC_REL)
				{
					if (value->type == VALUE_NUMBER)
					{
						SET_PROPERTY(svg_pathseg, Y1, (value->value.number));
						return PUT_SUCCESS;
					}
					return PUT_NEEDS_NUMBER;
				}
				break;
			case OP_ATOM_x2: // cubic*
				if (pstype == SVGDOMPathSeg::SVG_DOM_PATHSEG_CURVETO_CUBIC_ABS ||
					pstype == SVGDOMPathSeg::SVG_DOM_PATHSEG_CURVETO_CUBIC_REL ||
					pstype == SVGDOMPathSeg::SVG_DOM_PATHSEG_CURVETO_CUBIC_SMOOTH_ABS ||
					pstype == SVGDOMPathSeg::SVG_DOM_PATHSEG_CURVETO_CUBIC_SMOOTH_REL)
				{
					if (value->type == VALUE_NUMBER)
					{
						SET_PROPERTY(svg_pathseg, X2, (value->value.number));
						return PUT_SUCCESS;
					}
					return PUT_NEEDS_NUMBER;
				}
				break;
			case OP_ATOM_y2: // cubic*
				if (pstype == SVGDOMPathSeg::SVG_DOM_PATHSEG_CURVETO_CUBIC_ABS ||
					pstype == SVGDOMPathSeg::SVG_DOM_PATHSEG_CURVETO_CUBIC_REL ||
					pstype == SVGDOMPathSeg::SVG_DOM_PATHSEG_CURVETO_CUBIC_SMOOTH_ABS ||
					pstype == SVGDOMPathSeg::SVG_DOM_PATHSEG_CURVETO_CUBIC_SMOOTH_REL)
				{
					if (value->type == VALUE_NUMBER)
					{
						SET_PROPERTY(svg_pathseg, Y2, (value->value.number));
						return PUT_SUCCESS;
					}
					return PUT_NEEDS_NUMBER;
				}
				break;
			case OP_ATOM_r1: // arc{r,a}
				if (pstype == SVGDOMPathSeg::SVG_DOM_PATHSEG_ARC_ABS ||
					pstype == SVGDOMPathSeg::SVG_DOM_PATHSEG_ARC_REL)
				{
					if (value->type == VALUE_NUMBER)
					{
						SET_PROPERTY(svg_pathseg, R1, (value->value.number));
						return PUT_SUCCESS;
					}
					return PUT_NEEDS_NUMBER;
				}
				break;
			case OP_ATOM_r2: // arc{r,a}
				if (pstype == SVGDOMPathSeg::SVG_DOM_PATHSEG_ARC_ABS ||
					pstype == SVGDOMPathSeg::SVG_DOM_PATHSEG_ARC_REL)
				{
					if (value->type == VALUE_NUMBER)
					{
						SET_PROPERTY(svg_pathseg, R2, (value->value.number));
						return PUT_SUCCESS;
					}
					return PUT_NEEDS_NUMBER;
				}
				break;
			case OP_ATOM_angle: // arc{r,a}
				if (pstype == SVGDOMPathSeg::SVG_DOM_PATHSEG_ARC_ABS ||
					pstype == SVGDOMPathSeg::SVG_DOM_PATHSEG_ARC_REL)
				{
					if (value->type == VALUE_NUMBER)
					{
						SET_PROPERTY(svg_pathseg, Angle, (value->value.number));
						return PUT_SUCCESS;
					}
					return PUT_NEEDS_NUMBER;
				}
				break;
			case OP_ATOM_largeArcFlag: // arc{r,a}
				if (pstype == SVGDOMPathSeg::SVG_DOM_PATHSEG_ARC_ABS ||
					pstype == SVGDOMPathSeg::SVG_DOM_PATHSEG_ARC_REL)
				{
					if (value->type == VALUE_BOOLEAN)
					{
						SET_PROPERTY(svg_pathseg, LargeArcFlag, (value->value.boolean));
						return PUT_SUCCESS;
					}
					return PUT_NEEDS_BOOLEAN;
				}
				break;
			case OP_ATOM_sweepFlag: // arc{r,a}
				if (pstype == SVGDOMPathSeg::SVG_DOM_PATHSEG_ARC_ABS ||
					pstype == SVGDOMPathSeg::SVG_DOM_PATHSEG_ARC_REL)
				{
					if (value->type == VALUE_BOOLEAN)
					{
						SET_PROPERTY(svg_pathseg, SweepFlag, (value->value.boolean));
						return PUT_SUCCESS;
					}
					return PUT_NEEDS_BOOLEAN;
				}
				break;
			default:
				break;
			}
		}
		break;
		}
	}

	if (svg_item->IsA(SVG_DOM_ITEMTYPE_MATRIX))
	{
		SVGDOMMatrix* svg_matrix = static_cast<SVGDOMMatrix*>(svg_item);
		switch(property_name)
		{
		case OP_ATOM_a:
		{
			if (value->type == VALUE_NUMBER)
			{
				SET_PROPERTY(svg_matrix, Value, (0, value->value.number));
				return PUT_SUCCESS;
			}
			return PUT_NEEDS_NUMBER;
		}
		case OP_ATOM_b:
		{
			if (value->type == VALUE_NUMBER)
			{
				SET_PROPERTY(svg_matrix, Value, (1, value->value.number));
				return PUT_SUCCESS;
			}
			return PUT_NEEDS_NUMBER;
		}
		case OP_ATOM_c:
		{
			if (value->type == VALUE_NUMBER)
			{
				SET_PROPERTY(svg_matrix, Value, (2, value->value.number));
				return PUT_SUCCESS;
			}
			return PUT_NEEDS_NUMBER;
		}
		case OP_ATOM_d:
		{
			if (value->type == VALUE_NUMBER)
			{
				SET_PROPERTY(svg_matrix, Value, (3, value->value.number));
				return PUT_SUCCESS;
			}
			return PUT_NEEDS_NUMBER;
		}
		case OP_ATOM_e:
		{
			if (value->type == VALUE_NUMBER)
			{
				SET_PROPERTY(svg_matrix, Value, (4, value->value.number));
				return PUT_SUCCESS;
			}
			return PUT_NEEDS_NUMBER;
		}
		case OP_ATOM_f:
		{
			if (value->type == VALUE_NUMBER)
			{
				SET_PROPERTY(svg_matrix, Value, (5, value->value.number));
				return PUT_SUCCESS;
			}
			return PUT_NEEDS_NUMBER;
		}
		default:
			break;
		}
	}
#endif // SVG_FULL_11

	if (svg_item->IsA(SVG_DOM_ITEMTYPE_POINT))
	{
		SVGDOMPoint* svg_point = (SVGDOMPoint*)svg_item;
		BOOL is_current_translate = location.IsCurrentTranslate();

		switch(property_name)
		{
		case OP_ATOM_x:
			if (value->type == VALUE_NUMBER)
			{
				SET_PROPERTY(svg_point, X, (value->value.number));
				if(is_current_translate)
				{
					SVGDOM::OnCurrentTranslateChange(location.GetThisElement(), origining_runtime->GetFramesDocument(), svg_point);
				}
				return PUT_SUCCESS;
			}
			return PUT_NEEDS_NUMBER;
		case OP_ATOM_y:
			if (value->type == VALUE_NUMBER)
			{
				SET_PROPERTY(svg_point, Y, (value->value.number));
				if(is_current_translate)
				{
					SVGDOM::OnCurrentTranslateChange(location.GetThisElement(), origining_runtime->GetFramesDocument(), svg_point);
				}
				return PUT_SUCCESS;
			}
			return PUT_NEEDS_NUMBER;
		default:
			break;
		}
	}

#ifdef SVG_FULL_11
	if (svg_item->IsA(SVG_DOM_ITEMTYPE_PRESERVE_ASPECT_RATIO))
	{
		SVGDOMPreserveAspectRatio* svg_par = (SVGDOMPreserveAspectRatio*)svg_item;
		switch (property_name)
		{
		case OP_ATOM_align:
			if (value->type == VALUE_NUMBER)
			{
				SET_PROPERTY(svg_par, Align, ((unsigned short)value->value.number));
				return PUT_SUCCESS;
			}
			return PUT_NEEDS_NUMBER;
		case OP_ATOM_meetOrSlice:
			if (value->type == VALUE_NUMBER)
			{
				SET_PROPERTY(svg_par, MeetOrSlice, ((unsigned short)value->value.number));
				return PUT_SUCCESS;
			}
			return PUT_NEEDS_NUMBER;
		default:
			break;
		}
	}
#endif // SVG_FULL_11

	if (svg_item->IsA(SVG_DOM_ITEMTYPE_RECT))
	{
		SVGDOMRect* svg_rect = (SVGDOMRect*)svg_item;
		switch(property_name)
		{
		case OP_ATOM_x:
			if (value->type == VALUE_NUMBER)
			{
				SET_PROPERTY(svg_rect, X, (value->value.number));
				return PUT_SUCCESS;
			}
			return PUT_NEEDS_NUMBER;
		case OP_ATOM_y:
			if (value->type == VALUE_NUMBER)
			{
				SET_PROPERTY(svg_rect, Y, (value->value.number));
				return PUT_SUCCESS;
			}
			return PUT_NEEDS_NUMBER;
		case OP_ATOM_width:
			if (value->type == VALUE_NUMBER)
			{
				SET_PROPERTY(svg_rect, Width, (value->value.number));
				return PUT_SUCCESS;
			}
			return PUT_NEEDS_NUMBER;
		case OP_ATOM_height:
			if (value->type == VALUE_NUMBER)
			{
				SET_PROPERTY(svg_rect, Height, (value->value.number));
				return PUT_SUCCESS;
			}
			return PUT_NEEDS_NUMBER;
		default:
			break;
		}
	}

#ifdef SVG_FULL_11
	if (svg_item->IsA(SVG_DOM_ITEMTYPE_TRANSFORM))
	{
		switch(property_name)
		{
		case OP_ATOM_type:
		case OP_ATOM_matrix:
		case OP_ATOM_angle:
			return PUT_READ_ONLY;
		default:
			break;
		}
	}
#endif // SVG_FULL_11

#ifdef SVG_TINY_12
	if (svg_item->IsA(SVG_DOM_ITEMTYPE_PATH))
	{
		if(property_name == OP_ATOM_numberOfSegments)
		{
			return PUT_READ_ONLY;
		}
	}

	if (svg_item->IsA(SVG_DOM_ITEMTYPE_RGBCOLOR))
	{
		SVGDOMRGBColor* rgb_color = static_cast<SVGDOMRGBColor*>(svg_item);
		OP_STATUS status = OpStatus::ERR_NOT_SUPPORTED;
		switch(property_name)
		{
			case OP_ATOM_red:
			{
				if (value->type != VALUE_NUMBER)
					return PUT_NEEDS_NUMBER;
				else
					status = rgb_color->SetComponent(0, value->value.number);
			}
			break;
			case OP_ATOM_green:
			{
				if (value->type != VALUE_NUMBER)
					return PUT_NEEDS_NUMBER;
				else
					status = rgb_color->SetComponent(1, value->value.number);
			}
			break;
			case OP_ATOM_blue:
			{
				if (value->type != VALUE_NUMBER)
					return PUT_NEEDS_NUMBER;
				else
					status = rgb_color->SetComponent(2, value->value.number);
			}
			break;
		}

		if(OpStatus::IsSuccess(status))
		{
			return PUT_SUCCESS;
		}
		else
		{
			return PUT_FAILED;
		}
	}
#endif // SVG_TINY_12

	ES_PutState state = DOM_Object::PutName(property_name, value, origining_runtime);
	if (state == PUT_FAILED && !HaveNativeProperty())
	{
		// Someone putting a custom property on the object. We have to
		// keep this exact DOM object forever (and not GC it and
		// re-create it later on).
		SetIsSignificant();
		SetHaveNativeProperty();
		return PUT_FAILED_DONT_CACHE;
	}
	return state;
}
#undef SET_PROPERTY

#ifdef SVG_FULL_11
/* static */ int
DOM_SVGObject::newValueSpecifiedUnits(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_CHECK_ARGUMENTS("nn");
	DOM_THIS_OBJECT(obj, DOM_TYPE_SVG_OBJECT, DOM_SVGObject);

	if (data == 0)
	{
		SVGDOMAngle::UnitType unit_type = (SVGDOMAngle::UnitType)static_cast<int>(argv[0].value.number);
		double new_len = argv[1].value.number;
		SVGDOMAngle* angle = (SVGDOMAngle*)obj->GetSVGDOMItem();
		OP_BOOLEAN status = angle->NewValueSpecifiedUnits(unit_type, new_len);
		return status == OpStatus::ERR_NO_MEMORY ? ES_NO_MEMORY : ES_FAILED; /* ES_FAILED means OK here */
	}
	else if (data == 1)
	{
		SVGDOMLength::UnitType unit_type = (SVGDOMLength::UnitType)static_cast<int>(argv[0].value.number);
		double new_len = argv[1].value.number;
		SVGDOMLength* length = (SVGDOMLength*)obj->GetSVGDOMItem();
		OP_BOOLEAN status = length->NewValueSpecifiedUnits(unit_type, new_len);
		return status == OpStatus::ERR_NO_MEMORY ? ES_NO_MEMORY : ES_FAILED; /* ES_FAILED means OK here */
	}
	return ES_FAILED;
}

/* static */ int
DOM_SVGObject::convertToSpecifiedUnits(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_CHECK_ARGUMENTS("n");
	DOM_THIS_OBJECT(obj, DOM_TYPE_SVG_OBJECT, DOM_SVGObject);

	if (data == 0)
	{
		SVGDOMAngle::UnitType unit_type = (SVGDOMAngle::UnitType)static_cast<int>(argv[0].value.number);
		SVGDOMAngle* angle = (SVGDOMAngle*)obj->GetSVGDOMItem();
		OP_BOOLEAN status = angle->ConvertToSpecifiedUnits(unit_type);
		return status == OpStatus::ERR_NO_MEMORY ? ES_NO_MEMORY : ES_FAILED; /* ES_FAILED means OK here */
	}
	else if (data == 1)
	{
		SVGDOMLength::UnitType unit_type = (SVGDOMLength::UnitType)static_cast<int>(argv[0].value.number);
		SVGDOMLength* length = (SVGDOMLength*)obj->GetSVGDOMItem();
		Markup::AttrType attr_name = obj->location.GetAttr();
		NS_Type ns = obj->location.GetNS();
		OP_BOOLEAN status = length->ConvertToSpecifiedUnits(unit_type, obj->GetThisElement(),
															attr_name, ns);
		return status == OpStatus::ERR_NO_MEMORY ? ES_NO_MEMORY : ES_FAILED; /* ES_FAILED means OK here */
	}
	return ES_FAILED;
}

/* static */ int
DOM_SVGObject::matrixTransform(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_CHECK_ARGUMENTS("o");
	DOM_SVG_THIS_ITEM(point, SVG_DOM_ITEMTYPE_POINT, SVGDOMPoint);
	DOM_SVG_ARGUMENT_OBJECT(matrix, 0, SVG_DOM_ITEMTYPE_MATRIX, SVGDOMMatrix);

	SVGDOMItem* dom_item;
	CALL_FAILED_IF_ERROR(SVGDOM::CreateSVGDOMItem(SVG_DOM_ITEMTYPE_POINT, dom_item));
	OpAutoPtr<SVGDOMPoint> transformed_point(static_cast<SVGDOMPoint*>(dom_item));

	OP_BOOLEAN status = point->MatrixTransform(matrix, transformed_point.get());
	if (OpStatus::IsMemoryError(status))
		return ES_NO_MEMORY;

	DOM_SVGObject* result;
	CALL_FAILED_IF_ERROR(DOM_SVGObject::Make(result, transformed_point.get(), DOM_SVGLocation(), origining_runtime->GetEnvironment()));

	transformed_point.release();

	DOMSetObject(return_value, result);
	return ES_VALUE;
}

#endif // SVG_FULL_11

/* static */ int
DOM_SVGObject::matrixMethods(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_SVG_THIS_ITEM(matrix, SVG_DOM_ITEMTYPE_MATRIX, SVGDOMMatrix);

#ifdef SVG_FULL_11
	if (data < 3)
	{
		// translate, scaleNonUniform, rotateFromVector
		DOM_CHECK_ARGUMENTS("nn");
	}
	else if (data < 7)
	{
		// scale, rotate, skewX, skewY
		DOM_CHECK_ARGUMENTS("n");
	}
	else if (data < 8)
	{
		// multiply
		DOM_CHECK_ARGUMENTS("o");
	}
#endif // SVG_FULL_11

	SVGDOMItem* dom_item;
	CALL_FAILED_IF_ERROR(SVGDOM::CreateSVGDOMItem(SVG_DOM_ITEMTYPE_MATRIX, dom_item));
	OpAutoPtr<SVGDOMMatrix> result_matrix(static_cast<SVGDOMMatrix*>(dom_item));

	switch (data)
	{
#ifdef SVG_FULL_11
	case 0: // translate(n, n)
		matrix->Translate(argv[0].value.number, argv[1].value.number, result_matrix.get());
		break;
	case 1: // scaleNonUniform(n, n)
		matrix->ScaleNonUniform(argv[0].value.number, argv[1].value.number, result_matrix.get());
		break;
	case 2: // rotateFromVector(n, n)
	{
		OP_BOOLEAN could_rotate = matrix->RotateFromVector(argv[0].value.number, argv[1].value.number, result_matrix.get());
		if (OpBoolean::IS_FALSE == could_rotate)
			return DOM_CALL_SVGEXCEPTION(SVG_INVALID_VALUE_ERR);
	}
	break;
	case 3: // scale(n)
		matrix->Scale(argv[0].value.number, result_matrix.get());
		break;
	case 4: // rotate(n)
		matrix->Rotate(argv[0].value.number, result_matrix.get());
		break;
	case 5: // skewX(n)
		matrix->SkewX(argv[0].value.number, result_matrix.get());
		break;
	case 6: // skewY(n)
		matrix->SkewY(argv[0].value.number, result_matrix.get());
		break;
	case 7: // multiply(o)
	{
		DOM_SVG_ARGUMENT_OBJECT(second_matrix, 0, SVG_DOM_ITEMTYPE_MATRIX, SVGDOMMatrix);
		matrix->Multiply(second_matrix, result_matrix.get());
	}
	break;
#endif // SVG_FULL_11
	case 8: // inverse()
	{
		OP_BOOLEAN could_invert = matrix->Inverse(result_matrix.get());
		if (could_invert == OpBoolean::IS_FALSE)
			return DOM_CALL_SVGEXCEPTION(SVG_MATRIX_NOT_INVERTABLE);
	}
	break;
#ifdef SVG_FULL_11
	case 9: // flipX()
		matrix->FlipX(result_matrix.get());
		break;
	case 10: // flipY()
		matrix->FlipY(result_matrix.get());
		break;
#endif // SVG_FULL_11
	}

	DOM_SVGObject* result;
	CALL_FAILED_IF_ERROR(DOM_SVGObject::Make(result, result_matrix.get(), DOM_SVGLocation(), origining_runtime->GetEnvironment()));

	result_matrix.release();

	DOMSetObject(return_value, result);
	return ES_VALUE;
}

#ifdef SVG_TINY_12
/* static */ int
DOM_SVGObject::mutableMatrixMethods(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_SVG_THIS_ITEM(matrix, SVG_DOM_ITEMTYPE_MATRIX, SVGDOMMatrix);

	if (data == 0)
	{
		DOM_CHECK_ARGUMENTS("o");
		DOM_SVG_ARGUMENT_OBJECT(secondmatrix, 0, SVG_DOM_ITEMTYPE_MATRIX, SVGDOMMatrix);

		matrix->Multiply(secondmatrix, matrix);
	}
	else if (data == 1)
	{
		DOM_CHECK_ARGUMENTS("nn");

		matrix->Translate(argv[0].value.number, argv[1].value.number, matrix);
	}
	else if (data == 2)
	{
		DOM_CHECK_ARGUMENTS("n");

		matrix->Scale(argv[0].value.number, matrix);
	}
	else if (data == 3)
	{
		DOM_CHECK_ARGUMENTS("n");

		matrix->Rotate(argv[0].value.number, matrix);
	}
	else
		return ES_FAILED;

	DOMSetObject(return_value, this_object);
	return ES_VALUE;
}

/* static */ int
DOM_SVGObject::getComponent(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_CHECK_ARGUMENTS("n");
	DOM_SVG_THIS_ITEM(matrix, SVG_DOM_ITEMTYPE_MATRIX, SVGDOMMatrix);

	/* Range-check */
	if (argv[0].value.number < 0 ||
		argv[0].value.number > 5)
		return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);

	UINT32 idx = (UINT32)argv[0].value.number;
	DOMSetNumber(return_value, matrix->GetValue(idx));
	return ES_VALUE;
}
#endif // SVG_TINY_12

#ifdef SVG_FULL_11
/* static */ int
DOM_SVGObject::transformMethods(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_SVG_THIS_OBJECT_ITEM(obj, tfrm, SVG_DOM_ITEMTYPE_TRANSFORM, SVGDOMTransform);

	if (data == 0) // setRotate
	{
		DOM_CHECK_ARGUMENTS("nnn");
		tfrm->SetRotate(argv[0].value.number,
						argv[1].value.number,
						argv[2].value.number);
	}
	else if (data == 1) // setTranslate
	{
		DOM_CHECK_ARGUMENTS("nn");
		tfrm->SetTranslate(argv[0].value.number,
						   argv[1].value.number);
	}
	else if (data == 2) // setScale
	{
		DOM_CHECK_ARGUMENTS("nn");
		tfrm->SetScale(argv[0].value.number,
					   argv[1].value.number);
	}
	else if (data == 3) // setMatrix
	{
		DOM_CHECK_ARGUMENTS("o");
		DOM_SVG_ARGUMENT_OBJECT(matrix, 0, SVG_DOM_ITEMTYPE_MATRIX, SVGDOMMatrix);
		tfrm->SetMatrix(matrix);
	}
	else if (data == 4) // setSkewX
	{
		DOM_CHECK_ARGUMENTS("n");
		tfrm->SetSkewX(argv[0].value.number);
	}
	else if (data == 5) // setSkewY
	{
		DOM_CHECK_ARGUMENTS("n");
		tfrm->SetSkewY(argv[0].value.number);
	}

	obj->Invalidate();
	return ES_FAILED;
}

/* static */ int
DOM_SVGObject::setUri(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_CHECK_ARGUMENTS("s");
	DOM_SVG_THIS_ITEM(paint, SVG_DOM_ITEMTYPE_PAINT, SVGDOMPaint);
	paint->SetURI(argv[0].value.string);
	return ES_FAILED;
}

/* static */ int
DOM_SVGObject::setRGBColor(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_CHECK_ARGUMENTS("s");
	DOM_SVG_THIS_OBJECT_ITEM(obj, paint, SVG_DOM_ITEMTYPE_PAINT, SVGDOMPaint);
	OP_BOOLEAN status = paint->SetRGBColor(argv[0].value.string);
	if (OpBoolean::IS_FALSE == status)
		return DOM_CALL_SVGEXCEPTION(SVG_INVALID_VALUE_ERR);
	if (OpStatus::IsMemoryError(status))
		return ES_NO_MEMORY;
	OP_ASSERT(OpBoolean::IS_TRUE == status);
	obj->Invalidate();
	return ES_FAILED;
}

/* static */ int
DOM_SVGObject::setPaint(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_CHECK_ARGUMENTS("nSSS");
	DOM_SVG_THIS_OBJECT_ITEM(obj, paint, SVG_DOM_ITEMTYPE_PAINT, SVGDOMPaint);

	SVGDOMPaint::PaintType paint_type = (SVGDOMPaint::PaintType)(int)argv[0].value.number;

	OP_BOOLEAN status = OpBoolean::IS_TRUE;
	switch(paint_type)
	{
	case SVGDOMPaint::SVG_PAINTTYPE_RGBCOLOR:
		if (argv[1].type == VALUE_NULL &&
			argv[2].type == VALUE_STRING &&
			argv[3].type == VALUE_NULL)
			status = paint->SetPaint(SVGDOMPaint::SVG_PAINTTYPE_RGBCOLOR, NULL, argv[2].value.string, NULL);
		else
			return DOM_CALL_SVGEXCEPTION(SVG_INVALID_VALUE_ERR);
		break;
	case SVGDOMPaint::SVG_PAINTTYPE_URI:
		if (argv[1].type == VALUE_STRING &&
			argv[2].type == VALUE_NULL &&
			argv[3].type == VALUE_NULL)
			status = paint->SetPaint(SVGDOMPaint::SVG_PAINTTYPE_URI, argv[1].value.string, NULL, NULL);
		else
			return DOM_CALL_SVGEXCEPTION(SVG_INVALID_VALUE_ERR);
		break;
	case SVGDOMPaint::SVG_PAINTTYPE_URI_RGBCOLOR_ICCCOLOR:
		if (argv[1].type == VALUE_STRING &&
			argv[2].type == VALUE_STRING &&
			argv[3].type == VALUE_STRING)
			status = paint->SetPaint(SVGDOMPaint::SVG_PAINTTYPE_URI_RGBCOLOR_ICCCOLOR,
									 argv[1].value.string,
									 argv[2].value.string,
									 argv[3].value.string);
		else
			return DOM_CALL_SVGEXCEPTION(SVG_INVALID_VALUE_ERR);
		break;
	case SVGDOMPaint::SVG_PAINTTYPE_RGBCOLOR_ICCCOLOR:
		if (argv[1].type == VALUE_NULL &&
			argv[2].type == VALUE_STRING &&
			argv[3].type == VALUE_STRING)
			status = paint->SetPaint(SVGDOMPaint::SVG_PAINTTYPE_RGBCOLOR_ICCCOLOR,
									 NULL,
									 argv[2].value.string,
									 argv[3].value.string);
		else
			return DOM_CALL_SVGEXCEPTION(SVG_INVALID_VALUE_ERR);
		break;
	case SVGDOMPaint::SVG_PAINTTYPE_URI_NONE:
	case SVGDOMPaint::SVG_PAINTTYPE_URI_CURRENTCOLOR:
		if (argv[1].type == VALUE_STRING &&
			argv[2].type == VALUE_NULL &&
			argv[3].type == VALUE_NULL)
			status = paint->SetPaint(paint_type,
									 argv[1].value.string,
									 NULL,
									 NULL);
		else
			return DOM_CALL_SVGEXCEPTION(SVG_INVALID_VALUE_ERR);
		break;
	case SVGDOMPaint::SVG_PAINTTYPE_URI_RGBCOLOR:
		if (argv[1].type == VALUE_STRING &&
			argv[2].type == VALUE_STRING &&
			argv[3].type == VALUE_NULL)
			status = paint->SetPaint(SVGDOMPaint::SVG_PAINTTYPE_URI_RGBCOLOR,
									 argv[1].value.string,
									 argv[2].value.string,
									 NULL);
		else
			return DOM_CALL_SVGEXCEPTION(SVG_INVALID_VALUE_ERR);
		break;
	case SVGDOMPaint::SVG_PAINTTYPE_CURRENTCOLOR:
	case SVGDOMPaint::SVG_PAINTTYPE_NONE:
		if (argv[1].type == VALUE_NULL &&
			argv[2].type == VALUE_NULL &&
			argv[3].type == VALUE_NULL)
			status = paint->SetPaint(paint_type, NULL, NULL, NULL);
		else
			return DOM_CALL_SVGEXCEPTION(SVG_INVALID_VALUE_ERR);
		break;
	default:
		return DOM_CALL_SVGEXCEPTION(SVG_INVALID_VALUE_ERR);
	}

	if (status == OpBoolean::IS_FALSE)
		return DOM_CALL_SVGEXCEPTION(SVG_INVALID_VALUE_ERR);

	obj->Invalidate();
	return ES_FAILED;
}

/* static */ int
DOM_SVGObject::getFloatValue(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_CHECK_ARGUMENTS("n");
	DOM_SVG_THIS_ITEM(primitive_value, SVG_DOM_ITEMTYPE_CSSPRIMITIVEVALUE, SVGDOMCSSPrimitiveValue);

	SVGDOMCSSPrimitiveValue::UnitType type = (SVGDOMCSSPrimitiveValue::UnitType)(int)argv[0].value.number;
	if (type < SVGDOMCSSPrimitiveValue::SVG_DOM_CSS_UNKNOWN || type > SVGDOMCSSPrimitiveValue::SVG_DOM_CSS_RGBCOLOR)
		return DOM_CALL_DOMEXCEPTION(INVALID_ACCESS_ERR);

	double val;
	OP_BOOLEAN status = primitive_value->GetFloatValue(type, val);
	if (OpBoolean::IS_TRUE == status)
	{
		DOMSetNumber(return_value, val);
		return ES_VALUE;
	}
	else
	{
		return DOM_CALL_DOMEXCEPTION(INVALID_ACCESS_ERR);
	}
}
#endif // SVG_FULL_11

#ifdef SVG_TINY_12
/* static */ int
DOM_SVGObject::svgPathBuilder(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_SVG_THIS_ITEM(svgdompath, SVG_DOM_ITEMTYPE_PATH, SVGDOMPath);
	SVGPath* path = svgdompath->GetPath();

	switch(data)
	{
	case 0:
		{
			DOM_CHECK_ARGUMENTS("nn");
			path->MoveTo(argv[0].value.number, argv[1].value.number, FALSE);
		}
		break;
	case 1:
		{
			DOM_CHECK_ARGUMENTS("nn");
			path->LineTo(argv[0].value.number, argv[1].value.number, FALSE);
		}
		break;
	case 2:
		{
			DOM_CHECK_ARGUMENTS("nnnn");
			path->QuadraticCurveTo(argv[0].value.number, argv[1].value.number, argv[2].value.number, argv[3].value.number, FALSE, FALSE);
		}
		break;
	case 3:
		{
			DOM_CHECK_ARGUMENTS("nnnnnn");
			path->CubicCurveTo( argv[0].value.number, argv[1].value.number,
								argv[2].value.number, argv[3].value.number,
								argv[4].value.number, argv[5].value.number,
								FALSE, FALSE);
		}
		break;
	case 4:
		{
			path->Close();
		}
		break;
	case 5:
		{
			DOM_CHECK_ARGUMENTS("n");
			short segment;
			if(OpStatus::IsError(svgdompath->GetSegment((UINT32)argv[0].value.number, segment)))
			{
				return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);
			}

			DOMSetNumber(return_value, segment);
			return ES_VALUE;
		}
		break;
	case 6:
		{
			DOM_CHECK_ARGUMENTS("nn");
			double param;
			if(OpStatus::IsError(svgdompath->GetSegmentParam((UINT32)argv[0].value.number, (UINT32)argv[1].value.number, param)))
			{
				return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);
			}

			DOMSetNumber(return_value, param);
			return ES_VALUE;
		}
		break;
	default:
		break;
	}

	return ES_FAILED;
}
#endif // SVG_TINY_12

/* virtual */ void
DOM_SVGObject::Release()
{
#ifdef SVG_FULL_11
	in_list = NULL;
#endif // SVG_FULL_11
	location = DOM_SVGLocation();
}

#ifdef SVG_FULL_11
void
DOM_SVGObject::SetInList(DOM_SVGList* list)
{
	OP_ASSERT(list);
	in_list = list;
	location = list->Location();
}
#endif // SVG_FULL_11

/* static */ void
DOM_SVGObject_Prototype::ConstructL(ES_Object *object, DOM_Runtime::SVGObjectPrototype type, DOM_Runtime *runtime)
{
	switch(type)
	{
#ifdef SVG_FULL_11
	case DOM_Runtime::SVGANGLE_PROTOTYPE:
		DOM_SVGObject::ConstructDOMImplementationSVGObjectL(object, SVG_DOM_ITEMTYPE_ANGLE, runtime);
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGObject::newValueSpecifiedUnits, 0, "newValueSpecifiedUnits", NULL);
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGObject::convertToSpecifiedUnits, 0, "convertToSpecifiedUnits", NULL);
		break;
	case DOM_Runtime::SVGLENGTH_PROTOTYPE:
		DOM_SVGObject::ConstructDOMImplementationSVGObjectL(object, SVG_DOM_ITEMTYPE_LENGTH, runtime);
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGObject::newValueSpecifiedUnits, 1, "newValueSpecifiedUnits", NULL);
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGObject::convertToSpecifiedUnits, 1, "convertToSpecifiedUnits", NULL);
		break;
	case DOM_Runtime::SVGPOINT_PROTOTYPE:
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGObject::matrixTransform, "matrixTransform", NULL);
		break;
	case DOM_Runtime::SVGTRANSFORM_PROTOTYPE:
		DOM_SVGObject::ConstructDOMImplementationSVGObjectL(object, SVG_DOM_ITEMTYPE_TRANSFORM, runtime);
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGObject::transformMethods, 0, "setRotate", NULL);
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGObject::transformMethods, 1, "setTranslate", NULL);
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGObject::transformMethods, 2, "setScale", NULL);
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGObject::transformMethods, 3, "setMatrix", NULL);
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGObject::transformMethods, 4, "setSkewX", NULL);
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGObject::transformMethods, 5, "setSkewY", NULL);
		break;
#endif // SVG_FULL_11
	case DOM_Runtime::SVGMATRIX_PROTOTYPE:
#ifdef SVG_FULL_11
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGObject::matrixMethods, 0, "translate", NULL);
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGObject::matrixMethods, 1, "scaleNonUniform", NULL);
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGObject::matrixMethods, 2, "rotateFromVector", NULL);
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGObject::matrixMethods, 3, "scale", NULL);
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGObject::matrixMethods, 4, "rotate", NULL);
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGObject::matrixMethods, 5, "skewX", NULL);
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGObject::matrixMethods, 6, "skewY", NULL);
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGObject::matrixMethods, 7, "multiply", NULL);
#endif
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGObject::matrixMethods, 8, "inverse", NULL);
#ifdef SVG_FULL_11
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGObject::matrixMethods, 9, "flipX", NULL);
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGObject::matrixMethods, 10,"flipY", NULL);
#endif // SVG_FULL_11
#ifdef SVG_TINY_12
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGObject::getComponent, "getComponent", NULL);
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGObject::mutableMatrixMethods, 0, "mMultiply", NULL);
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGObject::mutableMatrixMethods, 1, "mTranslate", NULL);
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGObject::mutableMatrixMethods, 2, "mScale", NULL);
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGObject::mutableMatrixMethods, 3, "mRotate", NULL);
#endif // SVG_TINY_12
		break;

#ifdef SVG_FULL_11
	case DOM_Runtime::SVGPAINT_PROTOTYPE:
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGObject::setPaint, "setPaint", NULL);
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGObject::setUri, "setUri", NULL);
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGObject::setRGBColor, "setRGBColor", NULL);
		break;
	case DOM_Runtime::SVGCSSPRIMITIVEVALUE_PROTOTYPE:
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGObject::getFloatValue, "getFloatValue", NULL);
	case DOM_Runtime::SVGPATHSEG_PROTOTYPE:
		break;
#endif // SVG_FULL_11
#ifdef SVG_TINY_12
	case DOM_Runtime::SVGPATH_PROTOTYPE:
		DOM_SVGObject::ConstructDOMImplementationSVGObjectL(object, SVG_DOM_ITEMTYPE_PATH, runtime);
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGObject::svgPathBuilder, 0, "moveTo", NULL);
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGObject::svgPathBuilder, 1, "lineTo", NULL);
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGObject::svgPathBuilder, 2, "quadTo", NULL);
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGObject::svgPathBuilder, 3, "curveTo", NULL);
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGObject::svgPathBuilder, 4, "close", NULL);
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGObject::svgPathBuilder, 5, "getSegment", NULL);
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGObject::svgPathBuilder, 6, "getSegmentParam", NULL);
		break;
#endif // SVG_TINY_12
	}
}

/* static */ OP_STATUS
DOM_SVGObject_Prototype::Construct(ES_Object *object, DOM_Runtime::SVGObjectPrototype type, DOM_Runtime *runtime)
{
	TRAPD(status, ConstructL(object, type, runtime));
	return status;
}

#endif // SVG_DOM
