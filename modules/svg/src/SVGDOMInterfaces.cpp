/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef SVG_DOM

#include "modules/svg/svg_dominterfaces.h"

#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/AttrValueStore.h"
#include "modules/svg/src/SVGDocumentContext.h"
#include "modules/svg/src/svgdom/svgdomtransformimpl.h"
#include "modules/svg/src/svgdom/svgdomanimatedvalueimpl.h"
#include "modules/svg/src/svgdom/svgdommatriximpl.h"
#include "modules/svg/src/svgdom/svgdomstringimpl.h"
#include "modules/svg/src/svgdom/svgdomnumberimpl.h"
#include "modules/svg/src/svgdom/svgdompreserveaspectratioimpl.h"
#include "modules/svg/src/svgdom/svgdomenumerationimpl.h"
#include "modules/svg/src/svgdom/svgdomlengthimpl.h"
#include "modules/svg/src/svgdom/svgdomangleimpl.h"
#include "modules/svg/src/svgdom/svgdomrectimpl.h"
#include "modules/svg/src/svgdom/svgdompointimpl.h"
#include "modules/svg/src/svgdom/svgdompaintimpl.h"
#include "modules/svg/src/svgdom/svgdomcolorimpl.h"
#include "modules/svg/src/svgdom/svgdompathsegimpl.h"
#include "modules/svg/src/svgdom/svgdomstringlistimpl.h"
#include "modules/svg/src/svgdom/svgdompathseglistimpl.h"
#include "modules/svg/src/svgdom/svgdomlistimpl.h"
#include "modules/svg/src/svgdom/svgdompathimpl.h"
#include "modules/svg/src/svgdom/svgdomrgbcolorimpl.h"
#include "modules/svg/src/SVGManagerImpl.h"
#include "modules/svg/src/SVGObject.h"
#include "modules/svg/src/SVGTraverse.h"
#include "modules/svg/src/SVGChildIterator.h"
#include "modules/svg/src/SVGUtils.h"
#include "modules/svg/src/SVGAnimationContext.h"
#include "modules/svg/src/SVGDynamicChangeHandler.h"
#include "modules/svg/src/SVGFontSize.h"
#include "modules/svg/src/SVGMotionPath.h"

#include "modules/svg/src/animation/svganimationworkplace.h"

#include "modules/layout/box/box.h"
#include "modules/layout/cascade.h" // LayoutProperties

#ifdef SVG_FULL_11
static SVGObject*
RedirectObject(SVGObject* obj, short attr, NS_Type ns, SVGDOMItemType itemtype)
{
	SVGObject* target = obj;

	if (!obj)
		return NULL;

	if (ns == NS_SVG && attr == Markup::SVGA_ORIENT)
	{
		OP_ASSERT(obj->Type() == SVGOBJECT_ORIENT);
		SVGOrient* orient = static_cast<SVGOrient*>(obj);
		if (itemtype == SVG_DOM_ITEMTYPE_ANGLE)
			target = orient->GetAngle();
		else
		{
			OP_ASSERT(itemtype == SVG_DOM_ITEMTYPE_ENUM);
			target = orient->GetEnum();
		}
	}

	return target;
}

static OP_STATUS
LowGetAnimatedValue(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, SVGDOMItemType itemtype, SVGDOMItemType sub_itemtype, SVGDOMAnimatedValue*& value, Markup::AttrType attr_name, NS_Type ns)
{
	int ns_idx = (ns == NS_XLINK) ? NS_IDX_XLINK : NS_IDX_DEFAULT;

	SVGObject *base_obj, *anim_obj;
	RETURN_IF_ERROR(AttrValueStore::GetAttributeObjectsForDOM(elm, attr_name, ns_idx, &base_obj, &anim_obj));

	base_obj = RedirectObject(base_obj, attr_name, ns, itemtype);
	anim_obj = RedirectObject(anim_obj, attr_name, ns, itemtype);

	if (itemtype == SVG_DOM_ITEMTYPE_LIST && sub_itemtype == SVG_DOM_ITEMTYPE_TRANSFORM)
	{
		OP_ASSERT(base_obj->Type() == SVGOBJECT_VECTOR);
		SVGVector *base_vector = static_cast<SVGVector *>(base_obj);

		SVGTransform *anim_transform = NULL;
		if (anim_obj != base_obj)
		{
			OP_ASSERT(anim_obj->Type() == SVGOBJECT_TRANSFORM);
			anim_transform = static_cast<SVGTransform *>(anim_obj);
		}

		BOOL additive = ATTR_VAL_AS_NUM(elm->GetSpecialAttr(Markup::SVGA_ANIMATE_TRANSFORM_ADDITIVE, ITEM_TYPE_NUM,
															NUM_AS_ATTR_VAL(TRUE), SpecialNs::NS_SVG)) != FALSE;
		SVGDOMAnimatedValueTransformListImpl *animated_value = OP_NEW(SVGDOMAnimatedValueTransformListImpl, (base_vector, anim_transform, additive));
		if (!animated_value)
			return OpStatus::ERR_NO_MEMORY;

		value = animated_value;
	}
	else
	{
		SVGDOMAnimatedValueImpl* animated_value;
		RETURN_IF_ERROR(SVGDOMAnimatedValueImpl::Make(animated_value, base_obj, anim_obj, itemtype, sub_itemtype));
		value = animated_value;
	}

	return OpStatus::OK;
}
#endif // SVG_FULL_11

/* static */ OP_STATUS
SVGDOM::CreateSVGDOMItem(SVGDOMItemType object_type, SVGDOMItem*& obj)
{
	OP_STATUS status = OpStatus::OK;
	obj = NULL;

	switch(object_type)
	{
#ifdef SVG_FULL_11
		case SVG_DOM_ITEMTYPE_NUMBER:
			{
				SVGNumberObject* val = OP_NEW(SVGNumberObject, ());
				if (!val)
					return OpStatus::ERR_NO_MEMORY;

				SVGDOMNumberImpl* wrap = OP_NEW(SVGDOMNumberImpl, (val));
				if (!wrap)
					OP_DELETE(val);
				obj = wrap;
			}
			break;
#endif // SVG_FULL_11

		case SVG_DOM_ITEMTYPE_MATRIX:
			{
				SVGMatrixObject* val = OP_NEW(SVGMatrixObject, ());
				if (!val)
					return OpStatus::ERR_NO_MEMORY;

				SVGDOMMatrixImpl* wrap = OP_NEW(SVGDOMMatrixImpl, (val));
				if (!wrap)
					OP_DELETE(val);
				obj = wrap;
			}
			break;

#ifdef SVG_FULL_11
		case SVG_DOM_ITEMTYPE_TRANSFORM:
			{
				SVGTransform* val = OP_NEW(SVGTransform, (SVGTRANSFORM_MATRIX));
				if (!val)
					return OpStatus::ERR_NO_MEMORY;
				val->SetMatrix(1.0, 0.0, 0.0, 1.0, 0.0, 0.0); // Identity is default

				SVGDOMTransformImpl* wrap = OP_NEW(SVGDOMTransformImpl, (val));
				if (!wrap)
					OP_DELETE(val);
				obj = wrap;
			}
			break;

		case SVG_DOM_ITEMTYPE_LENGTH:
			{
				SVGLengthObject* val = OP_NEW(SVGLengthObject, ());
				if (!val)
					return OpStatus::ERR_NO_MEMORY;

				SVGDOMLengthImpl* wrap = OP_NEW(SVGDOMLengthImpl, (val));
				if (!wrap)
					OP_DELETE(val);
				obj = wrap;
			}
			break;
#endif // SVG_FULL_11

		case SVG_DOM_ITEMTYPE_RECT:
			{
				SVGRectObject* val = OP_NEW(SVGRectObject, ());
				if (!val)
					return OpStatus::ERR_NO_MEMORY;

				SVGDOMRectImpl* wrap = OP_NEW(SVGDOMRectImpl, (val));
				if (!wrap)
					OP_DELETE(val);
				obj = wrap;
			}
			break;

		case SVG_DOM_ITEMTYPE_POINT:
			{
				SVGPoint* val = OP_NEW(SVGPoint, ());
				if (!val)
					return OpStatus::ERR_NO_MEMORY;

				SVGDOMPointImpl* wrap = OP_NEW(SVGDOMPointImpl, (val));
				if (!wrap)
					OP_DELETE(val);
				obj = wrap;
			}
			break;
#ifdef SVG_FULL_11
		case SVG_DOM_ITEMTYPE_ANGLE:
			{
				SVGAngle* val = OP_NEW(SVGAngle, ());
				if (!val)
					return OpStatus::ERR_NO_MEMORY;

				SVGDOMAngleImpl* wrap = OP_NEW(SVGDOMAngleImpl, (val));
				if (!wrap)
					OP_DELETE(val);
				obj = wrap;
			}
			break;
#endif // SVG_FULL_11
#ifdef SVG_TINY_12
		case SVG_DOM_ITEMTYPE_PATH:
			{
				OpBpath* path;
				RETURN_IF_MEMORY_ERROR(OpBpath::Make(path, FALSE));

				SVGDOMPathImpl* wrap = OP_NEW(SVGDOMPathImpl, (path));
				if(!wrap)
					OP_DELETE(path);
				obj = wrap;
			}
			break;

		case SVG_DOM_ITEMTYPE_RGBCOLOR:
			{
				SVGPaintObject* val = OP_NEW(SVGPaintObject, (SVGPaint::RGBCOLOR));
				if (!val)
					return OpStatus::ERR_NO_MEMORY;

				SVGDOMRGBColorImpl* wrap = OP_NEW(SVGDOMRGBColorImpl, (val));
				if (!wrap)
					OP_DELETE(val);
				obj = wrap;
			}
			break;
#endif // SVG_TINY_12

		default:
			status = OpStatus::ERR_NOT_SUPPORTED;
			break;
	}

	if(status != OpStatus::ERR_NOT_SUPPORTED && !obj)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	return status;
}

/* static */ SVGDOMPathSeg*
SVGDOM::CreateSVGDOMPathSeg(int type)
{
#ifdef SVG_FULL_11
	SVGPathSegObject* seg = OP_NEW(SVGPathSegObject, ());
	if (!seg)
		return NULL;

	// Should have been checked by caller
	OP_ASSERT(type >= 1 && type <= 19);
	seg->seg.info.type = (SVGPathSeg::SVGPathSegType)type; /* These happen to be compatible */
	// Set the explicit flag for new moveto segments
	seg->seg.info.is_explicit = (seg->seg.info.type == SVGPathSeg::SVGP_MOVETO_ABS ||
								 seg->seg.info.type == SVGPathSeg::SVGP_MOVETO_REL) ? 1 : 0;
	seg->seg.x = 0;
	seg->seg.y = 0;
	seg->seg.x1 = 0;
	seg->seg.y1 = 0;
	seg->seg.x2 = 0;
	seg->seg.y2 = 0;
	seg->seg.info.large = 0; //FALSE
	seg->seg.info.sweep = 0; //FALSE

	SVGDOMPathSegImpl* wrap = OP_NEW(SVGDOMPathSegImpl, (seg));
	if(!wrap)
		OP_DELETE(seg);
	return wrap;
#else
	return NULL;
#endif // SVG_FULL_11
}

/* static */ OP_STATUS
SVGDOM::GetTrait(DOM_Environment *environment, HTML_Element* elm, Markup::AttrType attr, const uni_char *name, int ns_idx, BOOL presentation,
				 SVGDOMItemType itemType, SVGDOMItem** result, TempBuffer* resultstr, double* resultnum)
{
#ifdef SVG_TINY_12
	OP_STATUS err = OpStatus::ERR_NOT_SUPPORTED;
	NS_Type attr_ns = g_ns_manager->GetNsTypeAt(elm->ResolveNsIdx(ns_idx));
	HLDocProfile* hld_profile = NULL;
	CSS_decl* OP_MEMORY_VAR cd = NULL;
	SVGObject* obj = NULL;
	OpAutoPtr<SVGAttribute> defaultAttr(NULL);

	BOOL is_pres_attr = (attr_ns == NS_SVG &&
						 SVGUtils::IsPresentationAttribute(attr,
														   elm->GetNsType() == NS_SVG ?
														   elm->Type() :
														   Markup::HTE_UNKNOWN));

	if(is_pres_attr)
	{
		FramesDocument* doc = environment->GetFramesDocument();
		if (!doc)
			return OpStatus::ERR;

		hld_profile = doc->GetHLDocProfile();
		if(!hld_profile)
		{
			return OpStatus::ERR;
		}

		SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(elm);
		if (!doc_ctx || !doc_ctx->GetSVGImage()->IsInTree())
			return OpStatus::ERR;

		const uni_char* property_name = SVG_Lex::GetAttrString(attr);
		short propname = GetCSS_Property(property_name, uni_strlen(property_name));

		g_svg_manager_impl->SetPresentationValues(presentation);
		cd = LayoutProperties::GetComputedDecl(elm, propname, FALSE, hld_profile);
		g_svg_manager_impl->SetPresentationValues(TRUE);

		if(!cd)
			err = OpStatus::ERR_NO_MEMORY;
		else
			err = OpStatus::OK;
	}
	else if(SVGUtils::IsItemTypeTextAttribute(elm, attr, attr_ns))
	{
		err = OpStatus::OK; // handled by DOMGetAttribute below
	}
	else if(attr != ATTR_XML)
	{
		err = AttrValueStore::GetObject(elm, attr, ns_idx, FALSE, SVGOBJECT_UNKNOWN,
										&obj, !presentation ? SVG_ATTRFIELD_BASE : SVG_ATTRFIELD_DEFAULT);

		if(OpStatus::IsSuccess(err) && obj)
		{
			SVGObjectType object_type = SVGUtils::GetObjectType(elm->Type(),
														attr, attr_ns);
			if(object_type != obj->Type())
				return OpStatus::ERR;
		}

		// Perhaps move to SVGObject parser instead?
		if(OpStatus::IsSuccess(err) && obj && obj->Type() == SVGOBJECT_NAVREF)
		{
			SVGNavRef* navref = static_cast<SVGNavRef*>(obj);
			SVGNavRef::NavRefType nrt = navref->GetNavType();
			if (nrt == SVGNavRef::URI)
			{
				err = OpSVGStatus::INVALID_ARGUMENT;

				SVGURL nav_elm_url;
				if (OpStatus::IsSuccess(nav_elm_url.SetURL(navref->GetURI(), URL())))
				{
					if (SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(elm))
					{
						URL fullurl = nav_elm_url.GetURL(doc_ctx->GetURL(), elm);
						if(fullurl.RelName() != NULL && fullurl == doc_ctx->GetURL())
						{
							err = OpStatus::OK;
						}
					}
				}
			}
		}

		// time to find the default values for attributes.
		// Note: presentation attributes are not required here, they have their defaults in the cascade
		if (err == (OP_STATUS)OpSVGStatus::INVALID_ARGUMENT || // this is something that was in error, so get the default value
			(OpStatus::IsSuccess(err) && attr_ns == NS_SVG && !obj))
		{
			ItemType outType;
			void* outValue = NULL;
			const uni_char* default_value_string = NULL;

			switch(attr)
			{
				case Markup::SVGA_ACCUMULATE:
				case Markup::SVGA_BASEPROFILE:
				case Markup::SVGA_EDITABLE:
					default_value_string = UNI_L("none");
					break;
				case Markup::SVGA_ADDITIVE:
					default_value_string = UNI_L("replace");
					break;
				case Markup::SVGA_CALCMODE:
					default_value_string = elm->IsMatchingType(Markup::SVGE_ANIMATEMOTION, NS_SVG) ? UNI_L("paced") : UNI_L("linear");
					break;
				case Markup::SVGA_FILL:
					default_value_string = UNI_L("remove");
					break;
				case Markup::SVGA_NAV_RIGHT:
				case Markup::SVGA_NAV_NEXT:
				case Markup::SVGA_NAV_UP:
				case Markup::SVGA_NAV_UP_RIGHT:
				case Markup::SVGA_NAV_UP_LEFT:
				case Markup::SVGA_NAV_PREV:
				case Markup::SVGA_NAV_DOWN:
				case Markup::SVGA_NAV_DOWN_RIGHT:
				case Markup::SVGA_NAV_DOWN_LEFT:
				case Markup::SVGA_NAV_LEFT:
				case Markup::SVGA_FOCUSABLE:
				case Markup::SVGA_FOCUSHIGHLIGHT:
					default_value_string = UNI_L("auto");
					break;
				case Markup::SVGA_FONT_STYLE:
				case Markup::SVGA_FONT_WEIGHT:
				case Markup::SVGA_FONT_SIZE: // 'font-size' on <font-face> is not in SVGT12
					default_value_string = UNI_L("all");
					break;
				case Markup::SVGA_GRADIENTUNITS:
					default_value_string = UNI_L("objectBoundingBox");
					break;
				case Markup::SVGA_RESTART:
					default_value_string = UNI_L("always");
					break;
				case Markup::SVGA_TARGET:
					default_value_string = UNI_L("_self");
					break;
				case Markup::SVGA_TRANSFORM:
					default_value_string = UNI_L("matrix(1,0,0,1,0,0)");
					break;
				case Markup::SVGA_ZOOMANDPAN:
					default_value_string = UNI_L("magnify");
					break;
				case Markup::SVGA_VERSION:
					default_value_string = UNI_L("1.2");
					break;
			}

			if(default_value_string)
			{
				OP_STATUS parseErr = g_svg_manager_impl->ParseAttribute(elm,
								   NULL,
								   attr,
								   ns_idx,
								   default_value_string,
								   uni_strlen(default_value_string),
								   &outValue,
								   outType);

				if(outType == ITEM_TYPE_COMPLEX && outValue)
				{
					defaultAttr.reset((SVGAttribute*)outValue);
					obj = ((SVGAttribute*)outValue)->GetSVGObject();
				}

				if(OpStatus::IsError(parseErr) || outType != ITEM_TYPE_COMPLEX || !outValue)
				{
					err = OpStatus::ERR;
				}
				else
				{
					err = OpStatus::OK;
				}

				OP_ASSERT(OpStatus::IsError(parseErr) || (outValue && outType == ITEM_TYPE_COMPLEX)); // if this triggers we leak memory
			}
			else if(!resultnum) // float defaults are handled below
			{
				err = OpStatus::ERR;
			}
		}
	}

	if(err != (OP_STATUS)OpSVGStatus::INVALID_ARGUMENT && OpStatus::IsError(err))
		return err;

	if(resultstr)
	{
		if(is_pres_attr)
		{
			// this is ugly, but SVGT12 forces normalization for getTrait
			if(cd->GetProperty() == CSS_PROPERTY_display)
			{
				if(cd->TypeValue() == CSS_VALUE_none)
				{
					err = resultstr->Append("none");
				}
				else
				{
					err = resultstr->Append("inline");
				}
			}
			else
			{
				TRAP(err, CSS::FormatDeclarationL(resultstr, cd, FALSE, CSS_FORMAT_COMPUTED_STYLE));
			}
		}
		else
		{
			if(OpStatus::IsSuccess(err) && obj)
			{
				err = obj->GetStringRepresentation(resultstr);
			}
			else
			{
				err = resultstr->Append(elm->DOMGetAttribute(environment, attr, name, ns_idx, TRUE));
			}
		}
	}
	else if(result)
	{
		if(is_pres_attr)
		{
			if(attr_ns == NS_SVG && (attr == Markup::SVGA_FILL || attr == Markup::SVGA_STROKE))
			{
				SVGPropertyReference* ref;
				if(OpStatus::IsSuccess(SVGPaint::CloneFromCSSDecl(cd, ref, NULL)) && ref)
				{
					SVGPaint* paint = static_cast<SVGPaint*>(ref);
					SVGPaint::PaintType painttype = paint->GetPaintType();
					if (painttype == SVGPaint::RGBCOLOR || painttype == SVGPaint::RGBCOLOR_ICCCOLOR ||
						painttype == SVGPaint::URI_RGBCOLOR || painttype == SVGPaint::URI_RGBCOLOR_ICCCOLOR)
					{
						SVGPaintObject* po = OP_NEW(SVGPaintObject, (SVGPaint::RGBCOLOR));
						if(po)
						{
							po->GetPaint().Copy(*paint);

							SVGDOMRGBColorImpl* wrap = OP_NEW(SVGDOMRGBColorImpl, (po));
							if(!wrap)
							{
								OP_DELETE(po);
								err = OpStatus::ERR_NO_MEMORY;
							}
							else
								err = OpStatus::OK;

							*result = wrap;
						}
						else
						{
							err = OpStatus::ERR_NO_MEMORY;
						}
					}
					else
					{
						err = OpStatus::ERR;
					}

					SVGPropertyReference::DecRef(ref);
				}
			}
			else if(attr_ns == NS_SVG && (attr == Markup::SVGA_COLOR || attr == Markup::SVGA_STOP_COLOR || attr == Markup::SVGA_VIEWPORT_FILL || attr == Markup::SVGA_SOLID_COLOR))
			{
				if(cd->GetDeclType() == CSS_DECL_TYPE && cd->TypeValue() == CSS_VALUE_none)
				{
					err = OpStatus::ERR;
				}
				else
				{
					SVGColorObject* co = OP_NEW(SVGColorObject, ());
					if(co)
					{
						err = co->GetColor().SetFromCSSDecl(cd, NULL);
						if(OpStatus::IsSuccess(err))
						{
							SVGDOMRGBColorImpl* wrap = OP_NEW(SVGDOMRGBColorImpl, (co));
							if(!wrap)
							{
								OP_DELETE(co);
								err = OpStatus::ERR_NO_MEMORY;
							}
							else
								err = OpStatus::OK;

							*result = wrap;
						}
					}
					else
					{
						err = OpStatus::ERR_NO_MEMORY;
					}
				}
			}
			else
			{
				OP_ASSERT(!"FIXME: Implement CSS_decl -> SVGObject conversion");
				err = OpStatus::ERR_NOT_SUPPORTED;
			}
		}
		else
		{
			if(OpStatus::IsSuccess(err) && obj && attr_ns == NS_SVG)
			{
				*result = NULL;

				switch(itemType)
				{
					case SVG_DOM_ITEMTYPE_MATRIX:
						{
							SVGObjectType object_type = obj->Type();
							SVGObjectType vector_type = (object_type == SVGOBJECT_VECTOR ? static_cast<SVGVector*>(obj)->VectorType() : SVGOBJECT_UNKNOWN);

							if(object_type == SVGOBJECT_MATRIX ||
								object_type == SVGOBJECT_TRANSFORM ||
								(object_type == SVGOBJECT_VECTOR && (vector_type == SVGOBJECT_MATRIX || vector_type == SVGOBJECT_TRANSFORM)))
							{
								SVGMatrixObject* matrixObj = OP_NEW(SVGMatrixObject, ());
								if(matrixObj)
								{
									if (presentation)
									{
										SVGTrfmCalcHelper helper;
										helper.Setup(elm);

										if (helper.IsSet())
											if (!helper.HasRefTransform(matrixObj->matrix))
												helper.GetMatrix(matrixObj->matrix);
									}
									else
									{
										AttrValueStore::GetMatrix(elm, attr, matrixObj->matrix, SVG_ATTRFIELD_BASE);
									}

									SVGDOMMatrixImpl* wrap = OP_NEW(SVGDOMMatrixImpl, (static_cast<SVGMatrixObject*>(matrixObj)));
									if(!wrap)
										OP_DELETE(matrixObj);
									*result = wrap;
								}
								else
								{
									err = OpStatus::ERR_NO_MEMORY;
								}
							}
							else
								err = OpStatus::ERR;
						}
						break;
					case SVG_DOM_ITEMTYPE_RECT:
						if(obj->Type() == SVGOBJECT_RECT)
						{
							SVGDOMRectImpl* wrap = OP_NEW(SVGDOMRectImpl, (static_cast<SVGRectObject*>(obj)));
							*result = wrap;
						}
						else
							err = OpStatus::ERR;
						break;
					case SVG_DOM_ITEMTYPE_PATH:
						if(obj->Type() == SVGOBJECT_PATH)
						{
							SVGDOMPathImpl* wrap = OP_NEW(SVGDOMPathImpl, (static_cast<OpBpath *>(obj)));
							*result = wrap;
						}
						else
							err = OpStatus::ERR;
						break;
					case SVG_DOM_ITEMTYPE_RGBCOLOR:
						if(obj->Type() == SVGOBJECT_PAINT || obj->Type() == SVGOBJECT_COLOR)
						{
							SVGDOMRGBColorImpl* wrap = OP_NEW(SVGDOMRGBColorImpl, (obj));
							*result = wrap;
						}
						else
							err = OpStatus::ERR;
						break;
					default:
						OP_ASSERT(!"This itemType is not supported.");
						err = OpStatus::ERR_NOT_SUPPORTED;
						break;
				}

				if(*result)
				{
					err = OpStatus::OK;
				}
				else if(OpStatus::IsSuccess(err))
				{
					err = OpStatus::ERR_NO_MEMORY;
				}
			}
			else
			{
				SVGDOMItem* newitem;
				err = SVGDOM::CreateSVGDOMItem(itemType, newitem);
				*result = newitem;
			}
		}
	}
	else if(resultnum)
	{
		if(is_pres_attr)
		{
			if(cd->GetDeclType() == CSS_DECL_NUMBER)
			{
				*resultnum = cd->GetNumberValue(0);

				// kludge again, negative stroke-width values are allowed in cascade but disallowed in getTrait
				if ((attr == Markup::SVGA_STROKE_WIDTH || attr == Markup::SVGA_AUDIO_LEVEL) && *resultnum < 0 ||
					(attr == Markup::SVGA_AUDIO_LEVEL && *resultnum > 1))
				{
					*resultnum = 1.0; // default value
				}
				else if(attr == Markup::SVGA_STROKE_MITERLIMIT && *resultnum < 1)
				{
					*resultnum = 4.0; // default value
				}
			}
			else
			{
				err = OpStatus::ERR;
			}
		}
		else
		{
			BOOL hasDefaultValue = FALSE;

			// defaults for getFloat*Trait(NS) [attributes only]
			if(attr_ns == NS_SVG)
			{
				switch(attr)
				{
					case Markup::SVGA_CX:
					case Markup::SVGA_CY:
					case Markup::SVGA_HEIGHT:
					case Markup::SVGA_R:
					case Markup::SVGA_RX:
					case Markup::SVGA_RY:
					case Markup::SVGA_SNAPSHOTTIME:
					case Markup::SVGA_WIDTH:
					case Markup::SVGA_X:
					case Markup::SVGA_X1:
					case Markup::SVGA_X2:
					case Markup::SVGA_Y:
					case Markup::SVGA_Y1:
					case Markup::SVGA_Y2:
					case Markup::SVGA_OFFSET:
						// default is 0 for all of them
						hasDefaultValue = TRUE;
						break;
				}
			}

			if(OpStatus::IsSuccess(err) && obj)
			{
				switch(obj->Type())
				{
					case SVGOBJECT_NUMBER:
						{
							SVGNumberObject* val = static_cast<SVGNumberObject*>(obj);
							*resultnum = val->value.GetFloatValue();
						}
						break;
					case SVGOBJECT_LENGTH:
						{
							SVGLengthObject* val = static_cast<SVGLengthObject*>(obj);
							if(val->GetUnit() == CSS_NUMBER || val->GetUnit() == CSS_PX)
							{
								*resultnum = val->GetScalar().GetFloatValue();
							}
							else
							{
								err = OpStatus::ERR;
							}
						}
						break;
					case SVGOBJECT_FONTSIZE:
						{
							SVGFontSizeObject* val = static_cast<SVGFontSizeObject*>(obj);
							*resultnum = val->font_size.ResolvedLength().GetFloatValue();
						}
						break;
					case SVGOBJECT_PROXY:
						{
							SVGProxyObject* val = static_cast<SVGProxyObject*>(obj);
							BOOL success = FALSE;
							if (val->GetRealObject())
							{
								if(val->GetRealObject()->Type() == SVGOBJECT_ANIMATION_TIME)
								{
									SVGAnimationTimeObject* realval = static_cast<SVGAnimationTimeObject*>(val->GetRealObject());
									if(SVGAnimationTime::IsNumeric(realval->value))
									{
										*resultnum = SVGAnimationTime::ToSeconds(realval->value);
										success = TRUE;
									}
								}
								else if(val->GetRealObject()->Type() == SVGOBJECT_LENGTH)
								{
									SVGLengthObject* realval = static_cast<SVGLengthObject*>(val->GetRealObject());
									if(realval->GetUnit() == CSS_NUMBER || realval->GetUnit() == CSS_PX)
									{
										*resultnum = realval->GetScalar().GetFloatValue();
										success = TRUE;
									}
									else
									{
										err = OpStatus::ERR;
									}
								}
							}

							if(!success && hasDefaultValue)
							{
								*resultnum = 0;
							}
						}
						break;
					default:
						if(hasDefaultValue)
							*resultnum = 0;
						else
						{
							//OP_ASSERT(!"This objecttype is not supported.");
							err = OpStatus::ERR;
						}
				}

				if(OpStatus::IsSuccess(err))
				{
					// clip 'offset'
					if(attr == Markup::SVGA_OFFSET)
					{
						if(*resultnum > 1)
							*resultnum = 1;
						else if(*resultnum < 0)
							*resultnum = 0;
					}
					if(attr == Markup::SVGA_WIDTH || attr == Markup::SVGA_HEIGHT)
					{
						if(*resultnum < 0)
							*resultnum = 0;
					}
				}
			}
			else if(hasDefaultValue)
			{
				*resultnum = 0;
				err = OpStatus::OK;
			}
			else
			{
				err = OpStatus::ERR;
			}
		}
	}

	if(cd)
		OP_DELETE(cd);

	return err;
#else
	return OpStatus::ERR_NOT_SUPPORTED;
#endif // SVG_TINY_12
}

/* static */ OP_STATUS
SVGDOM::SetTrait(DOM_Environment *environment, HTML_Element* elm, Markup::AttrType attr, const uni_char *name, int ns_idx,
				 const uni_char *value, SVGDOMItem* value_object, double* value_num)
{
#ifdef SVG_TINY_12
	NS_Type attr_ns = g_ns_manager->GetNsTypeAt(elm->ResolveNsIdx(ns_idx));
	OP_STATUS err = OpStatus::ERR_NOT_SUPPORTED;

	if(attr == Markup::HA_NULL || (attr == Markup::HA_XML && (!name || name[0] == '\0')))
		return err;

	// If animation element is in the doctree we're not allowed to modify it
	if (SVGUtils::IsAnimationElement(elm))
	{
		if (FramesDocument* frm_doc = environment->GetFramesDocument())
			if (LogicalDocument* log_doc = frm_doc->GetLogicalDocument())
				if (HTML_Element* root = log_doc->GetRoot())
					if (root->IsAncestorOf(elm))
						return err;
	}


	if(value)
	{
		if(attr_ns == NS_SVG || attr_ns == NS_XLINK)
		{
			OP_ASSERT(elm->GetNsType() == NS_SVG);
			SVGObjectType object_type = SVGUtils::GetObjectType(elm->Type(),
																attr, attr_ns);
			SVGObject *result = NULL;

			err = OpStatus::ERR_NULL_POINTER;

			if(SVGUtils::IsItemTypeTextAttribute(elm, attr, attr_ns))
			{
				err = elm->DOMSetAttribute(environment, attr, NULL, ns_idx, value, uni_strlen(value), TRUE);
			}
			else if(OpStatus::IsSuccess(g_svg_manager_impl->ParseAttribute(elm, environment->GetFramesDocument(), object_type, attr, ns_idx, value, uni_strlen(value), &result)))
			{
				if(result->HasError())
				{
					OP_DELETE(result);
					err = OpStatus::ERR_NULL_POINTER;
				}
				else
					err = AttrValueStore::SetBaseObject(elm, attr, ns_idx, FALSE, result);
			}
			else if(result)
			{
				OP_DELETE(result);
			}
		}
		else
		{
			return elm->DOMSetAttribute(environment, attr, name, ns_idx, value, uni_strlen(value), TRUE);
		}
	}
	else if(value_object && attr != Markup::HA_XML && (attr_ns == NS_SVG || attr_ns == NS_XLINK))
	{
		SVGObject* clone = NULL;
		BOOL is_type_mismatch = FALSE;
		BOOL is_color = (attr == Markup::SVGA_COLOR || attr == Markup::SVGA_VIEWPORT_FILL || attr == Markup::SVGA_SOLID_COLOR ||
						 attr == Markup::SVGA_STOP_COLOR || attr == Markup::SVGA_LIGHTING_COLOR || attr == Markup::SVGA_FLOOD_COLOR);

		SVGObjectType object_type = SVGUtils::GetObjectType(elm->Type(),
															attr, attr_ns);

		if(attr_ns == NS_SVG && is_color && (value_object->GetSVGObject()->Type() != SVGOBJECT_COLOR))
		{
			UINT32 rgbcolor;
			RETURN_IF_ERROR(static_cast<SVGDOMRGBColorImpl*>(value_object)->GetRGBColor(rgbcolor));
			SVGColorObject* color_clone = OP_NEW(SVGColorObject, ());
			if (color_clone)
				color_clone->GetColor().SetRGBColor(rgbcolor);
			clone = color_clone;
		}
		else if(object_type == value_object->GetSVGObject()->Type())
		{
			clone = value_object->GetSVGObject()->Clone();
		}
		else if(object_type == SVGOBJECT_TRANSFORM && value_object->GetSVGObject()->Type() == SVGOBJECT_MATRIX)
		{
			SVGTransform* tfm = OP_NEW(SVGTransform, ());
			if(tfm)
			{
				tfm->SetMatrix(static_cast<SVGMatrixObject*>(value_object->GetSVGObject())->matrix);
				clone = tfm;
			}
		}
		else if(object_type == SVGOBJECT_VECTOR && attr_ns == NS_SVG &&
				SVGUtils::GetVectorObjectType(elm->Type(), attr) == SVGOBJECT_TRANSFORM &&
				value_object->GetSVGObject()->Type() == SVGOBJECT_MATRIX)
		{
			SVGVector* v = SVGUtils::CreateSVGVector(SVGOBJECT_TRANSFORM, elm->Type(), attr);
			if (v)
			{
				SVGTransform* tfm = OP_NEW(SVGTransform, ());
				if(!tfm)
				{
					OP_DELETE(tfm);
					OP_DELETE(v);
				}
				else
				{
					tfm->SetMatrix(static_cast<SVGMatrixObject*>(value_object->GetSVGObject())->matrix);
					err = v->Append(tfm);
				}

				if(OpStatus::IsSuccess(err))
				{
					clone = v;
				}
				else
				{
					OP_DELETE(tfm);
					OP_DELETE(v);
				}
			}
		}
		else
		{
			is_type_mismatch = TRUE;
		}

		if(is_type_mismatch)
		{
			err = OpStatus::ERR; // type mismatch err
		}
		else if(clone)
		{
			err = AttrValueStore::SetBaseObject(elm, attr, ns_idx, FALSE, clone);
			if(OpStatus::IsError(err))
			{
				OP_DELETE(clone);
			}
		}
		else
		{
			err = OpStatus::ERR_NO_MEMORY;
		}
	}
	else if(value_num)
	{
		if(attr != Markup::HA_XML && attr_ns == NS_SVG)
		{
			err = OpStatus::ERR;
			SVGObject* obj = NULL;
			OP_ASSERT(elm->GetNsType() == NS_SVG);
			Markup::Type element_type = elm->Type();
			SVGObjectType object_type = SVGUtils::GetObjectType(element_type,
																attr, attr_ns);
			BOOL special_xy = (object_type == SVGOBJECT_VECTOR &&
							   SVGUtils::IsTextClassType(element_type) &&
							   attr_ns == NS_SVG);

			if (object_type == SVGOBJECT_LENGTH ||
				object_type == SVGOBJECT_NUMBER ||
				object_type == SVGOBJECT_FONTSIZE ||
				special_xy)
			{
				err = AttrValueStore::GetObject(elm, attr, ns_idx, FALSE, object_type, &obj, SVG_ATTRFIELD_BASE);
				if(OpStatus::IsSuccess(err) && obj)
				{
					switch(object_type)
					{
						case SVGOBJECT_LENGTH:
							{
								SVGLengthObject* val = static_cast<SVGLengthObject*>(obj);
								val->SetScalar(*value_num);
								val->SetUnit(CSS_NUMBER);
							}
							break;
						case SVGOBJECT_NUMBER:
							{
								SVGNumberObject* val = static_cast<SVGNumberObject*>(obj);
								val->value = *value_num;
							}
							break;
						case SVGOBJECT_FONTSIZE:
							{
								SVGFontSizeObject* val = static_cast<SVGFontSizeObject*>(obj);
								SVGLength len(*value_num, CSS_NUMBER);
								err = val->font_size.SetLength(len);
							}
							break;
						case SVGOBJECT_VECTOR:
							{
								SVGVector* val = static_cast<SVGVector*>(obj);
								if(val->GetCount() > 1)
								{
									// remove all but the first item
									val->Remove(1, val->GetCount()-1);
								}
								if(val->VectorType() == SVGOBJECT_LENGTH)
								{
									SVGLengthObject* vval = static_cast<SVGLengthObject*>(val->Get(0));
									vval->SetScalar(*value_num);
									vval->SetUnit(CSS_NUMBER);
								}
							}
							break;
					}
				}
				else
				{
					switch(object_type)
					{
						case SVGOBJECT_LENGTH:
							{
								obj = OP_NEW(SVGLengthObject, (*value_num));
							}
							break;
						case SVGOBJECT_NUMBER:
							{
								obj = OP_NEW(SVGNumberObject, (*value_num));
							}
							break;
						case SVGOBJECT_FONTSIZE:
							{
								obj = OP_NEW(SVGFontSizeObject, ());
								if(obj)
								{
									SVGLength len(*value_num, CSS_NUMBER);
									static_cast<SVGFontSizeObject*>(obj)->font_size.SetLength(len);
								}
							}
							break;
						case SVGOBJECT_VECTOR:
							{
								obj = SVGUtils::CreateSVGVector(SVGOBJECT_LENGTH, element_type, attr);
								if (obj)
								{
									SVGObject* item = OP_NEW(SVGLengthObject, (*value_num));
									if(!item)
									{
										OP_DELETE(obj);
										obj = NULL;
									}
									else if(OpStatus::IsMemoryError(static_cast<SVGVector*>(obj)->Append(item)))
									{
										OP_DELETE(obj);
										OP_DELETE(item);
										obj = NULL;
									}
								}
							}
							break;
						default:
							return OpStatus::ERR_NOT_SUPPORTED;
					}

					if(!obj)
					{
						return OpStatus::ERR_NO_MEMORY;
					}

					err = AttrValueStore::SetBaseObject(elm, attr, ns_idx, FALSE, obj);
				}
			}
		}
		else if(attr_ns != NS_XLINK)
		{
			return elm->DOMSetNumericAttribute(environment, attr, name, ns_idx, *value_num);
		}
	}

	if(OpStatus::IsSuccess(err))
	{
		SVGDocumentContext *doc_ctx = AttrValueStore::GetSVGDocumentContext(elm);
		if(doc_ctx)
		{
			err = SVGDynamicChangeHandler::HandleAttributeChange(doc_ctx, elm, attr, attr_ns, FALSE);
		}
	}

	return err;
#else
	return OpStatus::ERR_NOT_SUPPORTED;
#endif // SVG_TINY_12
}

/* static */ OP_STATUS
SVGDOM::GetAnimatedValue(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, SVGDOMItemType itemType, SVGDOMAnimatedValue*& value, Markup::AttrType attr_name, NS_Type ns)
{
#ifdef SVG_FULL_11
	return LowGetAnimatedValue(elm, doc, itemType, SVG_DOM_ITEMTYPE_UNKNOWN, value, attr_name, ns);
#else
	return OpStatus::ERR_NOT_SUPPORTED;
#endif // SVG_FULL_11
}

/* static */ OP_STATUS
SVGDOM::GetAnimatedList(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, SVGDOMItemType listitemtype, SVGDOMAnimatedValue*& value, Markup::AttrType attr_name)
{
#ifdef SVG_FULL_11
	return LowGetAnimatedValue(elm, doc, SVG_DOM_ITEMTYPE_LIST, listitemtype, value, attr_name, NS_SVG);
#else
	return OpStatus::ERR_NOT_SUPPORTED;
#endif // SVG_TINY_12
}

/* static */ OP_STATUS
SVGDOM::GetAnimatedNumberOptionalNumber(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, SVGDOMAnimatedValue*& value, Markup::AttrType attr_name, UINT32 idx)
{
#ifdef SVG_FULL_11
	OP_ASSERT (attr_name == Markup::SVGA_FILTERRES ||
			   attr_name == Markup::SVGA_ORDER ||
			   attr_name == Markup::SVGA_KERNELUNITLENGTH ||
			   attr_name == Markup::SVGA_STDDEVIATION ||
			   attr_name == Markup::SVGA_RADIUS ||
			   attr_name == Markup::SVGA_BASEFREQUENCY);

	OP_ASSERT(idx == 0 || idx == 1);

	SVGDOMItemType item_type = SVG_DOM_ITEMTYPE_UNKNOWN;

	if (attr_name == Markup::SVGA_FILTERRES ||
		attr_name == Markup::SVGA_ORDER)
	{
		item_type = (idx == 0) ?
			SVG_DOM_ITEMTYPE_INTEGER_OPTIONAL_INTEGER_0 :
			SVG_DOM_ITEMTYPE_INTEGER_OPTIONAL_INTEGER_1;
	}
	else
	{
		item_type = (idx == 0) ?
			SVG_DOM_ITEMTYPE_NUMBER_OPTIONAL_NUMBER_0 :
			SVG_DOM_ITEMTYPE_NUMBER_OPTIONAL_NUMBER_1;
	}

	OP_ASSERT(item_type != SVG_DOM_ITEMTYPE_UNKNOWN);

	SVGObject* base_obj = NULL;
	SVGObject* anim_obj = NULL;
	AttrValueStore::GetAttributeObjectsForDOM(elm, attr_name, NS_IDX_SVG,
											  &base_obj, &anim_obj);

	SVGDOMAnimatedValueImpl* animated_value;
	RETURN_IF_ERROR(SVGDOMAnimatedValueImpl::Make(animated_value, base_obj,
												  anim_obj, item_type));
	value = animated_value;
	return OpStatus::OK;
#else
	return OpStatus::ERR_NOT_SUPPORTED;
#endif // SVG_FULL_11
}

/* static */ OP_STATUS
SVGDOM::GetViewPort(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, SVGDOMRect*& dom_rect)
{
	OP_ASSERT(elm);
	OP_ASSERT(elm->IsMatchingType(Markup::SVGE_SVG, NS_SVG));

	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(elm);
	if (doc_ctx)
	{
		if(!doc_ctx->HasViewPort())
		{
			SVGMatrix ctm;
			SVGRect vp;
			SVGNumberPair vpdim;
			SVGNumberPair vptrans;
			RETURN_IF_ERROR(SVGUtils::GetViewportForElement(elm, doc_ctx, vpdim, &vptrans, NULL));
			vp.Set(vptrans.x,vptrans.y,vpdim.x,vpdim.y);
			doc_ctx->UpdateViewport(vp);
		}

		dom_rect = OP_NEW(SVGDOMRectImpl, (doc_ctx->GetViewPort()));
		return dom_rect ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
	}
	else
	{
		dom_rect = NULL;
		return OpStatus::ERR;
	}
}

/* static */ OP_STATUS
SVGDOM::GetStringList(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc,
					  Markup::AttrType attr_name, SVGDOMStringList*& list)
{
#ifdef SVG_FULL_11
	OP_ASSERT(attr_name == Markup::SVGA_REQUIREDFEATURES ||
			  attr_name == Markup::SVGA_REQUIREDEXTENSIONS ||
			  attr_name == Markup::SVGA_SYSTEMLANGUAGE ||
			  attr_name == Markup::SVGA_VIEWTARGET);

	SVGObject *base_object;
	RETURN_IF_ERROR(AttrValueStore::GetAttributeObjectsForDOM(
						elm, attr_name, NS_IDX_SVG, &base_object, NULL));

	OP_ASSERT(base_object->Type() == SVGOBJECT_VECTOR);

	SVGVector *base_vector = static_cast<SVGVector *>(base_object);
	list = OP_NEW(SVGDOMStringListImpl, (base_vector));
	return list ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
#else
	return OpStatus::ERR_NOT_SUPPORTED;
#endif // SVG_FULL_11

}

/* static */ OP_STATUS
SVGDOM::AccessZoomAndPan(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc,
						 SVGZoomAndPanType& domzap, BOOL write)
{
#ifdef SVG_FULL_11
	SVGObject *base_obj;
	RETURN_IF_ERROR(AttrValueStore::GetAttributeObjectsForDOM(
						elm, Markup::SVGA_ZOOMANDPAN, NS_IDX_SVG, &base_obj, NULL));

	SVGEnum* enumval = static_cast<SVGEnum*>(base_obj);
	OP_ASSERT(enumval != NULL);

	if (write)
	{
		enumval->SetEnumType(SVGENUM_ZOOM_AND_PAN);

		if (domzap == SVG_DOM_SVG_ZOOMANDPAN_MAGNIFY ||
			domzap == SVG_DOM_SVG_ZOOMANDPAN_DISABLE)
		{
			enumval->SetEnumValue(domzap);
		}
		else
		{
			enumval->SetEnumValue(SVG_DOM_SVG_ZOOMANDPAN_UNKNOWN);
		}
		g_svg_manager->HandleSVGAttributeChange(doc, elm, Markup::SVGA_ZOOMANDPAN, NS_SVG, FALSE);
	}
	else /* read */
	{
		domzap = (SVGDOM::SVGZoomAndPanType)enumval->EnumValue();
	}

	return OpStatus::OK;
#else
	return OpStatus::ERR_NOT_SUPPORTED;
#endif // SVG_FULL_11
}

/* static */ OP_STATUS
SVGDOM::GetViewportElement(HTML_Element* elm, BOOL nearest, BOOL svg_only, HTML_Element*& viewport_element)
{
#ifdef SVG_FULL_11
	viewport_element = SVGUtils::GetViewportElement(elm, nearest, svg_only);
	return OpStatus::OK;
#else
	return OpStatus::ERR_NOT_SUPPORTED;
#endif // SVG_FULL_11
}

/* static */ double
SVGDOM::PixelUnitToMillimeterX(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc)
{
	// \TODO: Compute PixelUnitToMillimeterX
	return 0.28;
}

/* static */ double
SVGDOM::PixelUnitToMillimeterY(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc)
{
	// \TODO: Compute PixelUnitToMillimeterY
	return 0.28;
}

/* static */ double
SVGDOM::ScreenPixelToMillimeterX(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc)
{
	// \TODO: Compute ScreenPixelToMillimeterX
	return 0.28;
}

/* static */ double
SVGDOM::ScreenPixelToMillimeterY(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc)
{
	// \TODO: Compute ScreenPixelToMillimeterY
	return 0.28;
}

/* static */ BOOL
SVGDOM::UseCurrentView(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc)
{
	// \TODO: Find out the value of this.
	return FALSE;
}

/* static */ OP_STATUS
SVGDOM::GetCurrentScale(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, double& scale)
{
	if (!elm || !elm->IsMatchingType(Markup::SVGE_SVG, NS_SVG))
		return OpStatus::ERR;

	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(elm);
	if (doc_ctx != NULL)
	{
		scale = doc_ctx->GetCurrentScale().GetFloatValue();
		return OpStatus::OK;
	}
	return OpStatus::ERR;
}

/* static */ OP_STATUS
SVGDOM::SetCurrentScale(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, double scale)
{
	if (!elm || !elm->IsMatchingType(Markup::SVGE_SVG, NS_SVG))
		return OpStatus::ERR;

	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(elm);
	if (doc_ctx != NULL)
	{
		doc_ctx->UpdateZoom(SVGNumber(scale));
		return OpStatus::OK;
	}
	return OpStatus::ERR;
}

/* static */ OP_STATUS
SVGDOM::GetCurrentRotate(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, double& rotate)
{
#ifdef SVG_TINY_12
	if (!elm || !elm->IsMatchingType(Markup::SVGE_SVG, NS_SVG))
		return OpStatus::ERR;

	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(elm);
	if (doc_ctx != NULL)
	{
		rotate = doc_ctx->GetCurrentRotate().GetFloatValue();
		return OpStatus::OK;
	}
	return OpStatus::ERR;
#else
	return OpStatus::ERR_NOT_SUPPORTED;
#endif // SVG_TINY_12
}

/* static */ OP_STATUS
SVGDOM::SetCurrentRotate(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, double rotate)
{
#ifdef SVG_TINY_12
	if (!elm || !elm->IsMatchingType(Markup::SVGE_SVG, NS_SVG))
		return OpStatus::ERR;

	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(elm);
	if (doc_ctx != NULL)
	{
		doc_ctx->SetCurrentRotate(SVGNumber(rotate));
		return OpStatus::OK;
	}
	return OpStatus::ERR;
#else
	return OpStatus::ERR_NOT_SUPPORTED;
#endif // SVG_TINY_12
}

/* static */ OP_STATUS
SVGDOM::GetCurrentTranslate(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, SVGDOMPoint*& point)
{
	if (!elm || !elm->IsMatchingType(Markup::SVGE_SVG, NS_SVG))
		return OpStatus::ERR;

	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(elm);
	if (doc_ctx != NULL)
	{
		point = OP_NEW(SVGDOMPointImpl, (doc_ctx->GetCurrentTranslate()));
		return point ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::ERR;
}

/* static */ OP_STATUS
SVGDOM::OnCurrentTranslateChange(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, SVGDOMPoint* point)
{
	if (!elm || !elm->IsMatchingType(Markup::SVGE_SVG, NS_SVG) || !point)
		return OpStatus::ERR;

	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(elm);
	if (doc_ctx != NULL)
	{
		RETURN_IF_ERROR(doc_ctx->SendDOMEvent(SVGSCROLL, doc_ctx->GetSVGRoot()));
		return OpStatus::OK;
	}
	return OpStatus::ERR;
}

/* static */ OP_STATUS
SVGDOM::ForceRedraw(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc)
{
	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(elm);
	if (doc_ctx)
	{
		// Inside an SVG
		g_svg_manager_impl->ForceRedraw(doc_ctx);
	}
	return OpStatus::OK;
}

/* static */ OP_STATUS
SVGDOM::PauseAnimations(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc)
{
	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(elm);
	if (!doc_ctx || !doc)
		return OpStatus::ERR;

	// Create if needed, since it's possible to pause the timeline before we found any animation elements
	SVGAnimationWorkplace *animation_workplace = doc_ctx->AssertAnimationWorkplace();
	if (animation_workplace)
	{
		RETURN_IF_ERROR(animation_workplace->ProcessAnimationCommand(SVGAnimationWorkplace::ANIMATION_PAUSE));
	}
	return OpStatus::OK;
}

/* static */ OP_STATUS
SVGDOM::UnpauseAnimations(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc)
{
	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(elm);
	if (!doc_ctx || !doc)
		return OpStatus::ERR;

	SVGAnimationWorkplace *animation_workplace = doc_ctx->GetAnimationWorkplace();
	if (animation_workplace)
	{
		RETURN_IF_ERROR(animation_workplace->ProcessAnimationCommand(SVGAnimationWorkplace::ANIMATION_UNPAUSE));
	}
	return OpStatus::OK;
}

/* static */ OP_STATUS
SVGDOM::AnimationsPaused(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, BOOL& paused)
{
	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(elm);
	if (!doc_ctx)
		return OpStatus::ERR;

	SVGAnimationWorkplace *animation_workplace = doc_ctx->GetAnimationWorkplace();

	paused = FALSE;
	if (animation_workplace != NULL)
	{
		SVGAnimationWorkplace::AnimationStatus animation_status =
			animation_workplace->GetAnimationStatus();
		if (animation_status == SVGAnimationWorkplace::STATUS_PAUSED)
		{
		   paused = TRUE;
		}
	}

	return OpStatus::OK;
}

/* static */ OP_STATUS
SVGDOM::GetDocumentCurrentTime(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, double& current_time)
{
	SVGDocumentContext* docctx = AttrValueStore::GetSVGDocumentContext(elm);
	if(!docctx)
		return OpStatus::ERR;

	current_time = 0;

	SVGAnimationWorkplace *animation_workplace = docctx->GetAnimationWorkplace();
	if (animation_workplace)
	{
		current_time = SVGAnimationTime::ToSeconds(animation_workplace->DocumentTime());
	}

	return OpStatus::OK;
}

/* static */ OP_STATUS
SVGDOM::SetDocumentCurrentTime(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, double current_time)
{
	SVGDocumentContext* docctx = AttrValueStore::GetSVGDocumentContext(elm);
	if(!docctx)
		return OpStatus::ERR;

	SVGAnimationWorkplace *animation_workplace = docctx->GetAnimationWorkplace();
	if (animation_workplace)
	{
		animation_workplace->SetDocumentTime(SVGAnimationTime::FromSeconds(current_time));

		// FIXME: the following line might be possible to remove if we force an animation update
		// when someone accesses an animated value in the DOM (e.g foo.animVal.value, bar.getRGBColorPresentationTrait("fill"))
		docctx->GetSVGImage()->UpdateAnimations();

		animation_workplace->RequestUpdate();
	}

	return OpStatus::OK;
}

#ifdef SVG_FULL_11
static OP_STATUS GetIntersectedElementsList(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, SVGDOMRect* rect, BOOL enclosure, BOOL filter_list, HTML_Element* reference_element, OpVector<HTML_Element>& selected)
{
	if (rect == NULL)
		return OpStatus::ERR;

	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(elm);
	if (!doc_ctx || !doc_ctx->GetSVGImage()->IsInTree())
		return OpStatus::ERR;

	SVGRect intersection_rect(rect->GetX(), rect->GetY(),
							  rect->GetWidth(), rect->GetHeight());

	RETURN_IF_ERROR(g_svg_manager_impl->SelectElements(doc_ctx,
													   intersection_rect,
													   enclosure, selected));
	// If reference element is not null, filter results
	if (reference_element && filter_list)
	{
		UINT32 slot = 0;
		for (UINT32 i = 0; i < selected.GetCount(); i++)
		{
			if (reference_element->IsAncestorOf(selected.Get(i)))
			{
				selected.Replace(slot, selected.Get(i));
				slot++;
			}
		}

		// Clean out
		if (selected.GetCount() - slot > 0)
			selected.Remove(slot, selected.GetCount() - slot);
	}

	return OpStatus::OK;
}
#endif // SVG_FULL_11

/* static */ OP_STATUS
SVGDOM::GetIntersectionList(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, SVGDOMRect* rect, HTML_Element* reference_element, OpVector<HTML_Element>& selected)
{
#ifdef SVG_FULL_11
	return GetIntersectedElementsList(elm, doc, rect, FALSE, TRUE, reference_element, selected);
#else
	return OpStatus::ERR_NOT_SUPPORTED;
#endif // SVG_FULL_11
}

/* static */ OP_STATUS
SVGDOM::GetEnclosureList(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, SVGDOMRect* rect, HTML_Element* reference_element, OpVector<HTML_Element>& selected)
{
#ifdef SVG_FULL_11
	return GetIntersectedElementsList(elm, doc, rect, TRUE, TRUE, reference_element, selected);
#else
	return OpStatus::ERR_NOT_SUPPORTED;
#endif // SVG_FULL_11
}

/* static */ OP_STATUS
SVGDOM::CheckIntersection(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, SVGDOMRect* rect, HTML_Element* test_element, BOOL& intersects)
{
#ifdef SVG_FULL_11
	OpVector<HTML_Element> selected;

	RETURN_IF_ERROR(GetIntersectedElementsList(elm, doc, rect, FALSE, FALSE, test_element, selected));

	// Look for test_element in selection results
	if (selected.Find(test_element) >= 0)
		intersects = TRUE;
	else
		intersects = FALSE;

	return OpStatus::OK;
#else
	return OpStatus::ERR_NOT_SUPPORTED;
#endif // SVG_FULL_11
}

/* static */ OP_STATUS
SVGDOM::CheckEnclosure(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, SVGDOMRect* rect, HTML_Element* test_element, BOOL& intersects)
{
#ifdef SVG_FULL_11
	OpVector<HTML_Element> selected;

	RETURN_IF_ERROR(GetIntersectedElementsList(elm, doc, rect, TRUE, FALSE, test_element, selected));

	// Look for test_element in selection results
	if (selected.Find(test_element) >= 0)
		intersects = TRUE;
	else
		intersects = FALSE;

	return OpStatus::OK;
#else
	return OpStatus::ERR_NOT_SUPPORTED;
#endif // SVG_FULL_11
}

#ifdef SVG_FULL_11
static OP_STATUS GetNumberOfCharsInternal(HTML_Element* elm, SVGDocumentContext* doc_ctx, UINT32& numChars)
{
	OP_ASSERT(doc_ctx);
	SVGTextData data(SVGTextData::NUMCHARS);
	SVGNumberPair dummy_viewport;
	OP_STATUS status = SVGUtils::GetTextElementExtent(elm, doc_ctx, 0, -1, data,
													  dummy_viewport, NULL, FALSE);
	numChars = data.numchars;
	return status;
}
#endif // SVG_FULL_11

/* static */ OP_STATUS
SVGDOM::GetNumberOfChars(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, UINT32& numChars)
{
#ifdef SVG_FULL_11
	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(elm);
	if (!doc_ctx || !doc_ctx->GetSVGImage()->IsInTree())
	{
		// Not inside an SVG
		return OpStatus::ERR;
	}

	return GetNumberOfCharsInternal(elm, doc_ctx, numChars);
#else
	return OpStatus::ERR_NOT_SUPPORTED;
#endif // SVG_FULL_11
}

/* static */ OP_STATUS
SVGDOM::GetComputedTextLength(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, double& textlength)
{
#ifdef SVG_FULL_11
	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(elm);
	if (!doc_ctx || !doc_ctx->GetSVGImage()->IsInTree())
	{
		// Not inside an SVG
		return OpStatus::ERR;
	}

	SVGTextData data(SVGTextData::EXTENT);
	SVGNumberPair dummy_viewport;
	OP_STATUS status = SVGUtils::GetTextElementExtent(elm, doc_ctx, 0, -1, data, dummy_viewport);
	textlength = data.extent.GetFloatValue();
	return status;
#else
	return OpStatus::ERR_NOT_SUPPORTED;
#endif // SVG_FULL_11
}

/* static */ OP_STATUS
SVGDOM::GetSubStringLength(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, UINT32 firstCharIndex, UINT32 numChars, double& textlength)
{
#ifdef SVG_FULL_11
	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(elm);
	if (!doc_ctx || !doc_ctx->GetSVGImage()->IsInTree())
	{
		// Not inside an SVG
		return OpStatus::ERR;
	}

	UINT32 actualNumChars = 0;
	RETURN_IF_ERROR(GetNumberOfCharsInternal(elm, doc_ctx, actualNumChars));

	if(firstCharIndex >= actualNumChars)
		return OpStatus::ERR_OUT_OF_RANGE;

	if(firstCharIndex + numChars > actualNumChars)
	{
		numChars = actualNumChars-firstCharIndex;
	}

	SVGTextData data(SVGTextData::EXTENT);
	data.range = SVGTextRange(firstCharIndex, numChars);

	SVGNumberPair dummy_viewport;
	OP_STATUS status = SVGUtils::GetTextElementExtent(elm, doc_ctx, firstCharIndex, -1/*numChars*/,
													  data, dummy_viewport);
	textlength = data.extent.GetFloatValue();
	return status;
#else
	return OpStatus::ERR_NOT_SUPPORTED;
#endif // SVG_FULL_11
}

/* static */ OP_STATUS
SVGDOM::GetStartPositionOfChar(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, UINT32 charIndex, SVGDOMPoint*& startPos)
{
#ifdef SVG_FULL_11
	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(elm);
	if (!doc_ctx || !doc_ctx->GetSVGImage()->IsInTree())
	{
		// Not inside an SVG
		return OpStatus::ERR;
	}

	UINT32 numChars = 0;
	RETURN_IF_ERROR(GetNumberOfCharsInternal(elm, doc_ctx, numChars));

	if(charIndex >= numChars)
		return OpStatus::ERR_OUT_OF_RANGE;

	SVGTextData data(SVGTextData::STARTPOSITION);
	data.range = SVGTextRange(charIndex, 1);

	SVGNumberPair dummy_viewport;
	OP_STATUS status = SVGUtils::GetTextElementExtent(elm, doc_ctx, 0, /*charIndex*/-1,
													  data, dummy_viewport);
	RETURN_IF_ERROR(status);

	SVGDOMItem* dom_item;
	RETURN_IF_ERROR(CreateSVGDOMItem(SVG_DOM_ITEMTYPE_POINT, dom_item));
	SVGDOMPoint* point = static_cast<SVGDOMPoint*>(dom_item);

	point->SetX(data.startpos.x.GetFloatValue());
	point->SetY(data.startpos.y.GetFloatValue());

	startPos = point;

	return status;
#else
	return OpStatus::ERR_NOT_SUPPORTED;
#endif // SVG_FULL_11
}

/* static */ OP_STATUS
SVGDOM::GetEndPositionOfChar(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, UINT32 charIndex, SVGDOMPoint*& endPos)
{
#ifdef SVG_FULL_11
	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(elm);
	if (!doc_ctx || !doc_ctx->GetSVGImage()->IsInTree())
	{
		// Not inside an SVG
		return OpStatus::ERR;
	}

	UINT32 numChars = 0;
	RETURN_IF_ERROR(GetNumberOfCharsInternal(elm, doc_ctx, numChars));

	if(charIndex >= numChars)
		return OpStatus::ERR_OUT_OF_RANGE;

	SVGTextData data(SVGTextData::ENDPOSITION);
	SVGNumberPair dummy_viewport;
	OP_STATUS status = SVGUtils::GetTextElementExtent(elm, doc_ctx, 0, charIndex+1,
													  data, dummy_viewport);
	RETURN_IF_ERROR(status);

	SVGDOMItem* dom_item;
	RETURN_IF_ERROR(CreateSVGDOMItem(SVG_DOM_ITEMTYPE_POINT, dom_item));
	SVGDOMPoint* point = static_cast<SVGDOMPoint*>(dom_item);

	point->SetX(data.endpos.x.GetFloatValue());
	point->SetY(data.endpos.y.GetFloatValue());

	endPos = point;

	return status;
#else
	return OpStatus::ERR_NOT_SUPPORTED;
#endif // SVG_FULL_11
}

/* static */ OP_STATUS
SVGDOM::GetExtentOfChar(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, UINT32 charIndex, SVGDOMRect*& extent)
{
#ifdef SVG_FULL_11
	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(elm);
	if (!doc_ctx || !doc_ctx->GetSVGImage()->IsInTree())
	{
		// Not inside an SVG
		return OpStatus::ERR;
	}

	UINT32 numChars = 0;
	RETURN_IF_ERROR(GetNumberOfCharsInternal(elm, doc_ctx, numChars));

	if(charIndex >= numChars)
		return OpStatus::ERR_OUT_OF_RANGE;

	SVGTextData charWidth(SVGTextData::BBOX);
	charWidth.range = SVGTextRange(charIndex, 1);

	SVGNumberPair dummy_viewport;
	OP_STATUS status = SVGUtils::GetTextElementExtent(elm, doc_ctx, charIndex, 1, charWidth,
													  dummy_viewport, NULL, TRUE);

	SVGDOMItem* dom_item;
	RETURN_IF_ERROR(CreateSVGDOMItem(SVG_DOM_ITEMTYPE_RECT, dom_item));
	SVGDOMRect* rect = static_cast<SVGDOMRect*>(dom_item);

	SVGRect r = charWidth.bbox.GetSVGRect();
	rect->SetX(r.x.GetFloatValue());
	rect->SetY(r.y.GetFloatValue());
	rect->SetWidth(r.width.GetFloatValue());
	rect->SetHeight(r.height.GetFloatValue());

	extent = rect;
	return status;
#else
	return OpStatus::ERR_NOT_SUPPORTED;
#endif // SVG_FULL_11
}

/* static */ OP_STATUS
SVGDOM::GetRotationOfChar(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, UINT32 charIndex, double& rotation)
{
#ifdef SVG_FULL_11
	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(elm);
	if (!doc_ctx || !doc_ctx->GetSVGImage()->IsInTree())
	{
		// Not inside an SVG
		return OpStatus::ERR;
	}

	UINT32 numChars = 0;
	RETURN_IF_ERROR(GetNumberOfCharsInternal(elm, doc_ctx, numChars));

	if(charIndex >= numChars)
		return OpStatus::ERR_OUT_OF_RANGE;

	SVGTextData data(SVGTextData::ANGLE);
	data.range.index = charIndex;

	SVGNumberPair dummy_viewport;
	OP_STATUS status = SVGUtils::GetTextElementExtent(elm, doc_ctx, 0/*charIndex*/, 1,
													  data, dummy_viewport);
	rotation = data.angle.GetFloatValue();
	return status;
#else
	return OpStatus::ERR_NOT_SUPPORTED;
#endif // SVG_FULL_11
}

/* static */ OP_STATUS
SVGDOM::GetCharNumAtPosition(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, SVGDOMPoint* pos, long& charIndex)
{
#ifdef SVG_FULL_11
	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(elm);
	if (!doc_ctx || !doc_ctx->GetSVGImage()->IsInTree())
	{
		// Not inside an SVG
		return OpStatus::ERR;
	}

	if(!pos)
		return OpStatus::ERR_NULL_POINTER;

	SVGTextData data(SVGTextData::CHARATPOS);
	data.startpos.x = pos->GetX();
	data.startpos.y = pos->GetY();

	SVGNumberPair dummy_viewport;
	OP_STATUS status = SVGUtils::GetTextElementExtent(elm, doc_ctx, 0, -1,
													  data, dummy_viewport);
	charIndex = data.range.index;
	return status;
#else
	return OpStatus::ERR_NOT_SUPPORTED;
#endif // SVG_FULL_11
}

/* static */ OP_STATUS
SVGDOM::SelectSubString(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, UINT32 startCharIndex, UINT32 numChars)
{
#ifdef SVG_FULL_11
# ifdef SVG_SUPPORT_TEXTSELECTION
	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(SVGUtils::GetRootSVGElement(elm));
	if(!doc_ctx || !doc_ctx->GetSVGImage()->IsInTree())
		return OpStatus::ERR;
	UINT32 actualNumChars = 0;

	RETURN_IF_ERROR(GetNumberOfCharsInternal(elm, doc_ctx, actualNumChars));

	if(startCharIndex >= actualNumChars)
		return OpStatus::ERR_OUT_OF_RANGE;


	SVGTextSelection& sel = doc_ctx->GetTextSelection();
	if(!sel.IsSelecting())
	{
		return sel.DOMSetSelection(elm, startCharIndex, numChars);
	}
# endif // SVG_SUPPORT_TEXTSELECTION
	return OpStatus::ERR;
#else
	return OpStatus::ERR_NOT_SUPPORTED;
#endif // SVG_FULL_11

}

/* static */ OP_STATUS
SVGDOM::GetTotalLength(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, double& length)
{
#ifdef SVG_FULL_11
	OpBpath* path = NULL;
	length = 0;
	OP_STATUS status = AttrValueStore::GetPath(elm, Markup::SVGA_D, &path);
	if(OpStatus::IsSuccess(status) && path)
	{
		SVGMotionPath mp;
		status = mp.Set(path, -1);
		if(OpStatus::IsSuccess(status))
		{
			length = mp.GetLength().GetFloatValue();
		}
	}
	return status;
#else
	return OpStatus::ERR_NOT_SUPPORTED;
#endif // SVG_FULL_11
}

/* static */ OP_STATUS
SVGDOM::GetPointAtLength(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, double length, SVGDOMPoint*& point)
{
#ifdef SVG_FULL_11
	OpBpath* path = NULL;
	OP_STATUS status = AttrValueStore::GetPath(elm, Markup::SVGA_D, &path);
	if(OpStatus::IsSuccess(status))
	{
		SVGNumber x;
		SVGNumber y;

		if(path)
		{
			SVGMotionPath mp;
			status = mp.Set(path, -1);
			if(OpStatus::IsSuccess(status))
			{
				SVGMatrix matrix;
				mp.CalculateTransformAtDistance(SVGNumber(length), NULL, matrix);
				x = matrix[4];
				y = matrix[5];
			}
		}

		if(OpStatus::IsSuccess(status))
		{
			SVGDOMItem* dom_item;
			RETURN_IF_ERROR(CreateSVGDOMItem(SVG_DOM_ITEMTYPE_POINT, dom_item));
			point = static_cast<SVGDOMPoint*>(dom_item);

			point->SetX(x.GetFloatValue());
			point->SetY(y.GetFloatValue());
		}
	}

	return status;
#else
	return OpStatus::ERR_NOT_SUPPORTED;
#endif // SVG_FULL_11
}

/* static */ OP_STATUS
SVGDOM::GetPathSegAtLength(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, double length, UINT32& index)
{
#ifdef SVG_FULL_11
	OpBpath* path = NULL;
	OP_STATUS status = AttrValueStore::GetPath(elm, Markup::SVGA_D, &path);
	if(!path)
		status = OpStatus::ERR;

	if (OpStatus::IsSuccess(status))
	{
		SVGMotionPath mp;
		status = mp.Set(path, -1, TRUE /* account for moveTo:s */);
		if (OpStatus::IsSuccess(status))
			index = mp.GetSegmentIndexAtLength(length);
	}
	return status;
#else
	return OpStatus::ERR_NOT_SUPPORTED;
#endif // SVG_FULL_11
}

/* static */ OP_STATUS
SVGDOM::GetPathSegList(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, SVGDOMItem*& value, Markup::AttrType attr_type, BOOL base, BOOL normalized)
{
#ifdef SVG_FULL_11
	SVGObject *base_object;
	SVGObject *anim_object;
	AttrValueStore::GetAttributeObjectsForDOM(elm, attr_type, NS_IDX_SVG, &base_object, &anim_object);
	OpBpath * path = static_cast<OpBpath *>(base ? base_object : anim_object);
	OP_ASSERT(path != NULL);
	RETURN_IF_ERROR(path->SetUsedByDOM(TRUE));
	value = OP_NEW(SVGDOMPathSegListImpl, (path, normalized));
	return value ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
#else
	return OpStatus::ERR_NOT_SUPPORTED;
#endif // SVG_FULL_11
}

/* static */ OP_STATUS
SVGDOM::GetPointList(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, SVGDOMItem*& value, Markup::AttrType attr_type, BOOL base)
{
#ifdef SVG_FULL_11
	SVGObject *base_object;
	SVGObject *anim_object;
	AttrValueStore::GetAttributeObjectsForDOM(elm, attr_type, NS_IDX_SVG, &base_object, &anim_object);
	SVGVector* list = static_cast<SVGVector *>(base ? base_object : anim_object);
	value = OP_NEW(SVGDOMListImpl, (SVG_DOM_ITEMTYPE_POINT, list));
	return value ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
#else
	return OpStatus::ERR_NOT_SUPPORTED;
#endif // SVG_FULL_11
}

/* static */ OP_BOOLEAN
SVGDOM::GetBoundingBox(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, SVGDOMRect*& rect, int type)
{
	SVGElementContext* elem_ctx = AttrValueStore::GetSVGElementContext(elm);
	SVGRect bbox_rect;
	if (!elem_ctx || !elem_ctx->IsBBoxValid())
	{
		SVGNumberPair vp;
		SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(elm);
		if (doc_ctx && doc_ctx->GetSVGImage()->IsInTree())
		{
			// Need to go up to closest textroot element for the text subelements
			HTML_Element* traverseroot = elm;
			Markup::Type elm_type = elm->Type();
			if (SVGUtils::IsTextChildType(elm_type))
			{
				while (traverseroot)
				{
					if (SVGUtils::IsTextRootElement(traverseroot))
						break;

					traverseroot = traverseroot->Parent();
				}

				if (!traverseroot)
					return OpBoolean::IS_FALSE;
			}

			SVGMatrix ctm;
			RETURN_IF_ERROR(SVGUtils::GetViewportForElement(traverseroot->Parent() ? traverseroot->Parent() : traverseroot, doc_ctx, vp, NULL /* don't need x/y (?) */, &ctm));

			SVGNullCanvas dummy_canvas;
			dummy_canvas.SetDefaults(doc_ctx->GetRenderingQuality());
			dummy_canvas.ConcatTransform(ctm);

			SVGLogicalTreeChildIterator ltci;
			SVGBBoxUpdateObject bbu_object(&ltci);

			bbu_object.SetDocumentContext(doc_ctx);
			bbu_object.SetCurrentViewport(vp);
			bbu_object.SetCanvas(&dummy_canvas);

			RETURN_IF_ERROR(bbu_object.SetupResolver());

			bbu_object.SetInitialInvalidState(SVGElementContext::ComputeInvalidState(traverseroot));

			RETURN_IF_ERROR(SVGTraverser::Traverse(&bbu_object, traverseroot, NULL));

			if (!elem_ctx)
				elem_ctx = AttrValueStore::GetSVGElementContext(elm);
		}
	}

	if (elem_ctx)
	{
		if (elem_ctx->IsBBoxValid())
		{
			if(type == SVG_BBOX_NORMAL)
			{
				// NOTE: The bbox is in the userspace of the element already, there's no need to transform it.
				bbox_rect = elem_ctx->GetBBox().GetSVGRect();
			}
			else if(type == SVG_BBOX_SCREEN)
			{
				SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(elm);
				if (doc_ctx && doc_ctx->GetSVGImage()->IsInTree())
				{
					SVGMatrix matrix;
					OP_STATUS status = SVGUtils::GetElementCTM(elm, doc_ctx, &matrix, TRUE /* ScreenCTM */);
					if (OpStatus::IsSuccess(status))
					{
						bbox_rect = matrix.ApplyToBoundingBox(elem_ctx->GetBBox()).GetSVGRect();
					}
					else
						return OpBoolean::IS_FALSE;
				}
				else
					return OpBoolean::IS_FALSE;
			}
		}
		else
			return OpBoolean::IS_FALSE;
	}

	SVGDOMItem* dom_item;
	RETURN_IF_ERROR(CreateSVGDOMItem(SVG_DOM_ITEMTYPE_RECT, dom_item));
	rect = static_cast<SVGDOMRect*>(dom_item);

	rect->SetX(bbox_rect.x.GetFloatValue());
	rect->SetY(bbox_rect.y.GetFloatValue());
	rect->SetWidth(bbox_rect.width.GetFloatValue());
	rect->SetHeight(bbox_rect.height.GetFloatValue());

	return OpBoolean::IS_TRUE;
}

/* static */ OP_BOOLEAN
SVGDOM::GetCTM(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, SVGDOMMatrix*& matrix)
{
#ifdef SVG_FULL_11
	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(elm);
	if (!doc_ctx || !doc_ctx->GetSVGImage()->IsInTree())
	{
		// Not inside an SVG document
		return OpBoolean::IS_FALSE;
	}
	SVGMatrixObject* mat = OP_NEW(SVGMatrixObject, ());
	if (!mat)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status = SVGUtils::GetElementCTM(elm, doc_ctx, &mat->matrix, FALSE /* Non-ScreenCTM */);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(mat);
		return OpBoolean::IS_FALSE;
	}

	matrix = OP_NEW(SVGDOMMatrixImpl, (mat));
	if (!matrix)
	{
		OP_DELETE(mat);
		return OpStatus::ERR_NO_MEMORY;
	}

 	return OpBoolean::IS_TRUE;
#else
	return OpStatus::ERR_NOT_SUPPORTED;
#endif // SVG_FULL_11
}

/* static */ OP_BOOLEAN
SVGDOM::GetScreenCTM(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, SVGDOMMatrix*& matrix)
{
	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(elm);
	if (!doc_ctx || !doc_ctx->GetSVGImage()->IsInTree())
	{
		// Not inside an SVG document
		return OpBoolean::IS_FALSE;
	}
	SVGMatrixObject* mat = OP_NEW(SVGMatrixObject, ());
	if (!mat)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status = SVGUtils::GetElementCTM(elm, doc_ctx, &mat->matrix, TRUE /* ScreenCTM */);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(mat);
		return OpBoolean::IS_FALSE;
	}

	matrix = OP_NEW(SVGDOMMatrixImpl, (mat));
	if (!matrix)
	{
		OP_DELETE(mat);
		return OpStatus::ERR_NO_MEMORY;
	}

	HTML_Element* svgroot = SVGUtils::GetTopmostSVGRoot(elm);
	if(doc_ctx->GetLogicalDocument() && (doc_ctx->GetLogicalDocument()->GetDocRoot() != svgroot))
	{
		// This code offsets with boxrect in the case of compound documents
		if (VisualDevice* vis_dev = doc->GetVisualDevice())
		{
			RECT offsetRect = {0,0,0,0};
			BOOL success = FALSE;
			if (Box* box = svgroot->GetLayoutBox())
				success = box->GetRect(doc, CONTENT_BOX, offsetRect);
			if(success)
			{
				offsetRect.left = vis_dev->ScaleToScreen(offsetRect.left - vis_dev->GetRenderingViewX());
				offsetRect.top = vis_dev->ScaleToScreen(offsetRect.top - vis_dev->GetRenderingViewY());

				mat->matrix.MultTranslation(offsetRect.left, offsetRect.top);
			}
		}
	}

 	return OpBoolean::IS_TRUE;
}

/* static */ OP_BOOLEAN
SVGDOM::GetTransformToElement(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, HTML_Element* target_elm, SVGDOMMatrix*& matrix)
{
#ifdef SVG_FULL_11
	SVGDocumentContext* doc_ctx_from = AttrValueStore::GetSVGDocumentContext(elm);
	SVGDocumentContext* doc_ctx_target = AttrValueStore::GetSVGDocumentContext(target_elm);
	if (!doc_ctx_from || doc_ctx_from != doc_ctx_target ||
		!doc_ctx_from->GetSVGImage()->IsInTree())
	{
		// Not inside an SVG document or in different SVG documents
		return OpBoolean::IS_FALSE;
	}
	// result matrix = inv(target_elm.transform) * elm.transform * target_elm.transform * inv(target_elm.transform)
	// iff elm.transform and target_elm.transform are invertable
	SVGMatrix xfrm_to_elm;
	OP_BOOLEAN res = SVGUtils::GetTransformToElement(elm, target_elm,
													 doc_ctx_from, xfrm_to_elm);
	if (res == OpBoolean::IS_TRUE)
	{
		SVGMatrixObject* from_toinv = OP_NEW(SVGMatrixObject, ());
		if (!from_toinv)
			return OpStatus::ERR_NO_MEMORY;

		from_toinv->matrix.Copy(xfrm_to_elm);

		matrix = OP_NEW(SVGDOMMatrixImpl, (from_toinv));
		if (!matrix)
		{
			OP_DELETE(from_toinv);
			return OpStatus::ERR_NO_MEMORY;
		}
	}
	return res;
#else
	return OpStatus::ERR_NOT_SUPPORTED;
#endif // SVG_FULL_11
}

/* static */ OP_BOOLEAN
SVGDOM::GetPresentationAttribute(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, Markup::AttrType attr_name, SVGDOMItem*& item)
{
#if defined SVG_TINY_12 || defined SVG_FULL_11
	OP_ASSERT(elm != NULL);
	OP_ASSERT(elm->GetNsType() == NS_SVG);
	OP_ASSERT(doc != NULL);

	if (!SVGUtils::IsPresentationAttribute(attr_name, elm->Type()))
		return OpBoolean::IS_FALSE;

	Markup::Type elm_type = elm->Type();
	SVGObjectType obj_type = SVGUtils::GetObjectType(elm_type, attr_name, NS_SVG);

# ifdef SVG_FULL_11
	// Since GetPresentationAttributeForDOM will invalidate any stored
	// override string if it finds an object, make sure we leave early
	// if we know we will 'fail' the type check. This avoids
	// needlessly removing the override string.
	if (obj_type != SVGOBJECT_PAINT && obj_type != SVGOBJECT_COLOR)
	{
		item = NULL;
		return OpBoolean::IS_FALSE;
	}
# endif // SVG_FULL_11

	SVGObject* obj = AttrValueStore::GetPresentationAttributeForDOM(elm, attr_name, obj_type);
	if (!obj)
		return OpBoolean::IS_FALSE;

# ifdef SVG_FULL_11
	switch(obj_type)
	{
		case SVGOBJECT_PAINT:
			item = OP_NEW(SVGDOMPaintImpl, (static_cast<SVGPaintObject*>(obj)));
			break;
		case SVGOBJECT_COLOR:
			item = OP_NEW(SVGDOMColorImpl, (static_cast<SVGColorObject*>(obj)));
			break;
		default:
			// XXX: Implement GetPresentationAttribute for lengths and
			// other presentation attributes?
			item = NULL;
			return OpBoolean::IS_FALSE;
	}
# else
	OP_ASSERT(!"Implement GetPresentationAttribute for SVGT12.");
# endif // SVG_FULL_11

	if (!item)
		return OpStatus::ERR_NO_MEMORY;

	return OpBoolean::IS_TRUE;
#else
	return OpBoolean::IS_FALSE;
#endif // SVG_TINY_12 || SVG_FULL_11
}

/* static */ OP_STATUS
SVGDOM::BeginElement(HTML_Element *elm, SVG_DOCUMENT_CLASS *doc, double offset)
{
	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(elm);
	if (!doc_ctx)
	{
		// Not inside an SVG document
		return OpStatus::ERR;
	}

	SVGAnimationWorkplace *animation_workplace = doc_ctx->GetAnimationWorkplace();
	if (animation_workplace)
	{
		return animation_workplace->BeginElement(elm, SVGAnimationTime::FromSeconds(offset));
	}
	else
	{
		return OpStatus::ERR;
	}
}

/* static */ OP_STATUS
SVGDOM::EndElement(HTML_Element *elm, SVG_DOCUMENT_CLASS *doc, double offset)
{
	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(elm);
	if (!doc_ctx)
	{
		// Not inside an SVG document
		return OpStatus::ERR;
	}

	SVGAnimationWorkplace *animation_workplace = doc_ctx->GetAnimationWorkplace();
	if (animation_workplace)
	{
		return animation_workplace->EndElement(elm, SVGAnimationTime::FromSeconds(offset));
	}
	else
	{
		return OpStatus::ERR;
	}
}

#ifdef SVG_FULL_11
// Helper used below
static OP_STATUS SetNumberOptionalNumber(SVGVector* vect, double x, double y)
{
	SVGNumberObject* x_val = OP_NEW(SVGNumberObject, (x));
	if (!x_val)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status;
	if (vect->GetCount() < 1)
		status = vect->Insert(0, x_val);
	else
		status = vect->Replace(0, x_val);

	if (OpStatus::IsError(status))
	{
		OP_DELETE(x_val);
		return status;
	}

	SVGNumberObject* y_val = OP_NEW(SVGNumberObject, (y));
	if (!y_val)
		return OpStatus::ERR_NO_MEMORY;

	if (vect->GetCount() < 2)
		status = vect->Insert(1, y_val);
	else
		status = vect->Replace(1, y_val);

	if (OpStatus::IsError(status))
	{
		OP_DELETE(y_val);
		return status;
	}

	return OpStatus::OK;
}
#endif // SVG_FULL_11

/* static */ OP_STATUS
SVGDOM::SetFilterRes(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc,
					 double filter_res_x, double filter_res_y)
{
#ifdef SVG_FULL_11
	SVGObject* obj = NULL;
	RETURN_IF_ERROR(AttrValueStore::GetAttributeObjectsForDOM(elm, Markup::SVGA_FILTERRES, NS_IDX_SVG,
															  &obj, NULL));

	OP_ASSERT(obj->Type() == SVGOBJECT_VECTOR);
	SVGVector *vect = static_cast<SVGVector *>(obj);

	RETURN_IF_ERROR(SetNumberOptionalNumber(vect, filter_res_x, filter_res_y));
	g_svg_manager->HandleSVGAttributeChange(doc, elm, Markup::SVGA_FILTERRES, NS_SVG, FALSE);

	return OpStatus::OK;
#else
	return OpStatus::ERR_NOT_SUPPORTED;
#endif // SVG_FULL_11
}

/* static */ OP_STATUS
SVGDOM::SetStdDeviation(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc,
						double std_dev_x, double std_dev_y)
{
#ifdef SVG_FULL_11
	SVGObject* obj = NULL;
	RETURN_IF_ERROR(AttrValueStore::GetAttributeObjectsForDOM(elm, Markup::SVGA_STDDEVIATION, NS_IDX_SVG,
															  &obj, NULL));

	OP_ASSERT(obj->Type() == SVGOBJECT_VECTOR);
	SVGVector *vect = static_cast<SVGVector *>(obj);

	RETURN_IF_ERROR(SetNumberOptionalNumber(vect, std_dev_x, std_dev_y));
	g_svg_manager->HandleSVGAttributeChange(doc, elm, Markup::SVGA_STDDEVIATION, NS_SVG, FALSE);

	return OpStatus::OK;
#else
	return OpStatus::ERR_NOT_SUPPORTED;
#endif // SVG_FULL_11
}

/* static */ OP_STATUS
SVGDOM::GetInstanceRoot(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, BOOL animated, HTML_Element*& root)
{
#ifdef SVG_FULL_11
	if(!elm->IsMatchingType(Markup::SVGE_USE, NS_SVG))
		return OpStatus::ERR;

	BOOL has_built;
	if (animated)
	{
		has_built = SVGUtils::HasBuiltNormalShadowTree(elm);
	}
	else
	{
		has_built = SVGUtils::HasBuiltBaseShadowTree(elm);
	}

	if(!has_built)
	{
		SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(elm);
		if(!doc_ctx)
			return OpStatus::ERR;
		RETURN_IF_ERROR(SVGUtils::CreateShadowRoot(NULL, doc_ctx, elm,
			elm, animated));
	}

    HTML_Element* instanceroot = elm->FirstChild();
	Markup::Type elm_type = animated ? Markup::SVGE_ANIMATED_SHADOWROOT : Markup::SVGE_BASE_SHADOWROOT;

	while (instanceroot && !instanceroot->IsMatchingType(elm_type, NS_SVG))
	{
		instanceroot = instanceroot->Suc();
	}

	if (instanceroot)
	{
		root = instanceroot;
		return OpStatus::OK;
	}

	return OpStatus::ERR;
#else
	return OpStatus::ERR_NOT_SUPPORTED;
#endif // SVG_FULL_11
}

/* static */ OP_STATUS
SVGDOM::HasFeature(const uni_char* featurestring, const uni_char* version, BOOL& supported)
{
	BOOL supported_version = TRUE;
	if (version && *version)
	{
		supported_version = uni_str_eq(version, "1.0") || uni_str_eq(version, "1.1") || uni_str_eq(version, "1.2");
	}
	supported = supported_version && SVGUtils::HasFeature(featurestring, uni_strlen(featurestring));
	return OpStatus::OK;
}

/* static */ OP_STATUS
SVGDOM::SetOrient(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, SVGDOMAngle* angle)
{
#ifdef SVG_FULL_11
	SVGObject* base_obj = NULL;
	RETURN_IF_ERROR(AttrValueStore::GetAttributeObjectsForDOM(elm, Markup::SVGA_ORIENT, NS_IDX_SVG,
															  &base_obj, NULL));
	if (!base_obj)
	{
		// No base object - create one
		RETURN_IF_ERROR(AttrValueStore::CreateDefaultAttributeObject(elm, Markup::SVGA_ORIENT, NS_IDX_SVG,
																	 FALSE, base_obj));
		OP_STATUS status = AttrValueStore::SetBaseObject(elm, Markup::SVGA_ORIENT, NS_IDX_SVG,
														 FALSE, base_obj);
		if (OpStatus::IsError(status))
		{
			OP_DELETE(base_obj);
			return status;
		}
	}

	OP_ASSERT(base_obj);
	OP_ASSERT(base_obj->Type() == SVGOBJECT_ORIENT);
	SVGOrient* orient = static_cast<SVGOrient*>(base_obj);

	if (angle)
	{
		SVGAngleType angle_unit = SVGANGLE_DEG;
		switch (angle->GetUnitType())
		{
		case SVGDOMAngle::SVG_ANGLETYPE_RAD:
			angle_unit = SVGANGLE_RAD;
			break;
		case SVGDOMAngle::SVG_ANGLETYPE_GRAD:
			angle_unit = SVGANGLE_GRAD;
			break;
		}
		SVGAngle new_angle_val(SVGNumber(angle->GetValue()), angle_unit);
		orient->SetOrientType(SVGORIENT_ANGLE);
		RETURN_IF_ERROR(orient->SetAngle(new_angle_val));
	}
	else
	{
		orient->SetOrientType(SVGORIENT_AUTO);
	}

	g_svg_manager->HandleSVGAttributeChange(doc, elm, Markup::SVGA_ORIENT, NS_SVG, FALSE);

	return OpStatus::OK;
#else
	return OpStatus::ERR_NOT_SUPPORTED;
#endif // SVG_FULL_11
}

/* static */ OP_BOOLEAN
SVGDOM::HasExtension(const uni_char* uri)
{
	// In the future we may want to support some extensions here.
	return OpBoolean::IS_FALSE;
}

/* static */ UINT32
SVGDOM::Serial(HTML_Element* elm, Markup::AttrType attr, NS_Type ns)
{
	int ns_idx = (ns == NS_XLINK) ? NS_IDX_XLINK : NS_IDX_SVG;

	SVGAttribute* svg_attr = AttrValueStore::GetSVGAttr(elm, attr, ns_idx);
	return (svg_attr != NULL) ? svg_attr->GetSerial() : 0;
}

/* static */ OP_STATUS
SVGDOM::GetAnimationTargetElement(HTML_Element* anim_elm, SVG_DOCUMENT_CLASS *doc,
								  HTML_Element*& target_elm)
{
	if (!SVGUtils::IsAnimationElement(anim_elm))
		return OpStatus::ERR;

	SVGTimingInterface* timed_element_ctx = AttrValueStore::GetSVGTimingInterface(anim_elm);
	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(anim_elm);
	if (timed_element_ctx && doc_ctx)
	{
		target_elm = timed_element_ctx->GetDefaultTargetElement(doc_ctx);
		return target_elm ? OpStatus::OK : OpStatus::ERR;
	}
	else
		return OpStatus::ERR;
}

/* static */ OP_STATUS
SVGDOM::GetAnimationStartTime(HTML_Element* timed_element, SVG_DOCUMENT_CLASS *doc,
							  double& start_time)
{
	if (!SVGUtils::IsTimedElement(timed_element))
		return OpStatus::ERR;

	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(timed_element);
	if (!doc_ctx)
		return OpStatus::ERR;

	SVGAnimationWorkplace *animation_workplace = doc_ctx->GetAnimationWorkplace();

	SVGTimingInterface* timed_element_ctx = AttrValueStore::GetSVGTimingInterface(timed_element);
	if (timed_element_ctx && animation_workplace)
	{
		SVGAnimationSchedule &schedule = timed_element_ctx->AnimationSchedule();
		SVGAnimationInterval *active_interval = schedule.GetActiveInterval(animation_workplace->DocumentTime());
		if (active_interval && animation_workplace->DocumentTime() < active_interval->End())
		{
			start_time = SVGAnimationTime::ToSeconds(active_interval->Begin());
			return OpStatus::OK;
		}
	}

	return OpStatus::ERR;
}

/* static */ OP_STATUS
SVGDOM::GetAnimationCurrentTime(HTML_Element* timed_element, SVG_DOCUMENT_CLASS *doc, double& current_time)
{
	if (!SVGUtils::IsTimedElement(timed_element))
		return OpStatus::ERR;

	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(timed_element);
	if (!doc_ctx)
		return OpStatus::ERR;

	SVGAnimationWorkplace *animation_workplace = doc_ctx->GetAnimationWorkplace();

	if (animation_workplace)
	{
		current_time = SVGAnimationTime::ToSeconds(animation_workplace->DocumentTime());
		return OpStatus::OK;
	}

	return OpStatus::ERR;
}

/* static */ OP_STATUS
SVGDOM::GetAnimationSimpleDuration(HTML_Element* timed_element, SVG_DOCUMENT_CLASS *doc, double& duration)
{
	if (!SVGUtils::IsTimedElement(timed_element))
		return OpStatus::ERR;

	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(timed_element);
	if (!doc_ctx)
		return OpStatus::ERR;

	SVGAnimationWorkplace *animation_workplace = doc_ctx->GetAnimationWorkplace();

	SVGTimingInterface* timed_element_ctx = AttrValueStore::GetSVGTimingInterface(timed_element);
	if (timed_element_ctx && animation_workplace)
	{
		SVGAnimationSchedule &schedule = timed_element_ctx->AnimationSchedule();
		SVGAnimationInterval *active_interval = schedule.GetActiveInterval(animation_workplace->DocumentTime());
		if (active_interval)
		{
			duration = SVGAnimationTime::ToSeconds(active_interval->SimpleDuration());
			return OpStatus::OK;
		}
	}

	return OpStatus::ERR;
}

/* static */ OP_STATUS
SVGDOM::GetBoundingClientRect(HTML_Element *elm, OpRect &rect)
{
	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(elm);
	if (!doc_ctx)
		return OpStatus::ERR;

	FramesDocument* frm_doc = doc_ctx->GetDocument();
	SVGDOMRect *domrect;

	// FIXME: Maybe separate out the getting rect-part from the creation of the SVGDOMRect part
	if(OpBoolean::IS_TRUE == SVGDOM::GetBoundingBox(elm, frm_doc, domrect, SVG_BBOX_SCREEN))
	{
		rect.x = static_cast<int>(domrect->GetX());
		rect.y = static_cast<int>(domrect->GetY());
		rect.width = static_cast<int>(domrect->GetWidth());
		rect.height = static_cast<int>(domrect->GetHeight());
		OP_DELETE(domrect);

		HTML_Element* root = doc_ctx->GetSVGRoot();
		if (root)
		{
			int root_left, root_top, dummy;
			if (frm_doc && OpStatus::IsSuccess(root->DOMGetPositionAndSize(frm_doc->GetDOMEnvironment(), HTML_Element::DOM_PS_BORDER, root_left, root_top, dummy, dummy)))
			{
				rect.OffsetBy(root_left, root_top);
			}
		}

		return OpStatus::OK;
	}

	return OpStatus::ERR;
}

#endif // SVG_DOM
