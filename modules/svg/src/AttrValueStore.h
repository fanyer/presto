/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef _ATTR_VALUE_STORE_
#define _ATTR_VALUE_STORE_

#ifdef SVG_SUPPORT

#include "modules/svg/src/SVGObject.h"
#include "modules/svg/src/SVGAttribute.h"
#include "modules/svg/src/SVGValue.h"
#include "modules/svg/src/SVGMatrix.h"
#include "modules/svg/src/OpBpath.h"
#include "modules/svg/src/SVGInternalEnum.h"
#include "modules/svg/src/SVGElementStateContext.h"
#include "modules/svg/src/SVGMatrix.h"
#include "modules/svg/svg.h"

class HTML_Element;
class URL;

class SVGVector;
class SVGFontSize;
class SVGRepeatCount;
class SVGFontSizeObject;

/**
 * This class is here to help with figuring out the transform to apply to an element.
 */
class SVGTrfmCalcHelper
{
public:
	SVGTrfmCalcHelper() : m_transform_set(0) {}

	enum MatrixSelector
	{
		BASE_TRANSFORM_MATRIX = 0,
		ANIM_TRANSFORM_MATRIX,
		MOTION_TRANSFORM_MATRIX,
		REF_TRANSFORM_MATRIX,

		TRANSFORM_COUNT
	};

	void Setup(HTML_Element* e);

	void SetTransformMatrix(MatrixSelector type, const SVGMatrix& matrix)
	{
		OP_ASSERT((unsigned)type < TRANSFORM_COUNT);

		AddTransform(type);
		m_transform[type].Copy(matrix);
	}

	void operator=(const SVGTrfmCalcHelper& other)
	{
		if (&other != this)
		{
			op_memcpy(this, &other, sizeof(SVGTrfmCalcHelper));
		}
	}

	void GetMatrix(SVGMatrix& matrix) const;
	BOOL IsSet() const { return m_transform_set != 0; }
	void Reset() { m_transform_set = 0; }

	void SetRefTransform(const SVGMatrix& translation)
	{
		SetTransformMatrix(REF_TRANSFORM_MATRIX, translation);
	}
	BOOL HasRefTransform(SVGMatrix &matrix) const
	{
		if (HasTransform(REF_TRANSFORM_MATRIX))
		{
			matrix.Copy(m_transform[REF_TRANSFORM_MATRIX]);
			return TRUE;
		}
		return FALSE;
	}

private:
	BOOL HasTransform(MatrixSelector type) const { return (m_transform_set & (1 << type)) != 0; }
	void AddTransform(MatrixSelector type) { m_transform_set |= (1 << type); }

	// FIXME: Consider using UninitializedSVGMatrix for these, since
	// the loaded default identity matrix is never used
	SVGMatrix m_transform[TRANSFORM_COUNT];
	unsigned int m_transform_set;
};

/**
 * There are animation values and base values. In the default mode the
 * animation value is fetched, with fallback to the base value. There
 * is also the possibility to get/modify the base/animation value
 * specifically.
 *
 * You can set new objects too, which will automatically destroy the
 * old one. The ownership of the new object will then be passed to the
 * HTML_Element.
 *
 */
class AttrValueStore
{
public:
	/**
	 * Get an object from an attribute.
	 *
	 * This method returns the active object for this attribute, which
	 * means the animated object if the attribute is animated or the
	 * base object otherwise. If no object exists for this attribute,
	 * 'obj' is set to NULL and OpStatus::OK is returned. If the
	 * object exists but contains a parsing error, an error code is
	 * returned in addition to the object.

	 * @param e The element to fetch the object from.
	 * @param attr_type The attribute type to fetch the object from
	 * @param object_type The type of object to fetch. SVGOBJECT_UNKNOWN means don't care and no filtering are performed.
	 * @param obj The object pointer is stored at the location of this
	 * pointer.
	 * @param field_type The field the fetch. SVG_ATTRFIELD_DEFAULT
	 * fetched anim with base fallback. SVG_ATTRFIELD_ANIM only
	 * fetches the animation value, SVG_ATTRFIELD_BASE only fetches
	 * the base value.
	 * @param anim_attribute_type This is from the 'attributeType'
	 * attribute on animation elements.
	 * It specifies what object to get when animating.
	 * @return The error flag set when parsing the object, except
	 * OpStatus::ERR_NO_MEMORY which would have been taken care of
	 * already.
	 */
	static OP_STATUS GetObject(HTML_Element *e, Markup::AttrType attr_type, int ns_idx,
							   BOOL is_special,
							   SVGObjectType object_type, SVGObject** obj,
							   SVGAttributeField field_type = SVG_ATTRFIELD_DEFAULT,
							   SVGAttributeType anim_attribute_type = SVGATTRIBUTE_AUTO);

	/**
	 * Fetch a specific type of object. See other GetObject
	 * implementation. This one calls that one with namespace index set to
	 * NS_IDX_SVG and is_special=FALSE.
	 */
	static OP_STATUS GetObject(HTML_Element *e, Markup::AttrType attr_type,
							   SVGObjectType object_type, SVGObject** obj,
							   SVGAttributeField field_type = SVG_ATTRFIELD_DEFAULT,
							   SVGAttributeType anim_attribute_type = SVGATTRIBUTE_AUTO)
	{
		return GetObject(e, attr_type, NS_IDX_SVG, FALSE, object_type,
						 obj, field_type, anim_attribute_type);
	}

#if 0
	/**
	 * Get an object from an attribute as DOM would see it.
	 *
	 * The differences between this function and GetDefaultObject is
	 * that this function return uninitialized objects AND that is
	 * allows the caller to choose between base or animation objects.
	 *
	 * Uninitialized objects are created when DOM requests to read an
	 * property corresponding to an attribute, and that attribute does
	 * not exists. Then an uninitialized object is created and
	 * returned to DOM with a dummy value. This dummy value should not
	 * be visible in rendering until it has been assigned to a
	 * value. Then the value is not uninitialized anymore, and
	 * fetchable with GetDefaultObject.
	 */
	static OP_STATUS GetObjectForDOM(HTML_Element *e, int attr_type, int ns_idx,
									 SVGObjectType object_type, SVGObject*& obj,
									 SVGObject* def);
#endif // 0

	/**
	 * Gets an SVGAttribute value from the given element. This is meant to be a wrapper to the HTML_Element::GetAttr method,
	 * since ITEM_TYPE_COMPLEX can contain other complex attributes that are not SVGAttributes, such as the style attribute.
	 */
	static SVGAttribute* GetSVGAttr(HTML_Element* elm, Markup::AttrType attr_name, int ns_idx, BOOL is_special = FALSE);

	/**
	 * Get object of an attribute. If the attribute doesn't exists, it
	 * creates a unintialized base object along with a new attribute.
	 *
	 * Leave base_obj or anim_obj if you don't want to know the result
	 * these.
	 *
	 * Handles magic about how some anim_obj is stored in other
	 * attributes compared to the base_obj (animateTransform).
	 *
	 * If no animation value exists, the 'anim_obj' will be assigned
	 * the same value as 'base_obj'.
	 */
	static OP_STATUS GetAttributeObjectsForDOM(HTML_Element *e, Markup::AttrType attr_type,
											   int ns_idx,
											   SVGObject **base_obj,
											   SVGObject **anim_obj);

	/**
	 * Get object from a presentation attribute. If the attribute does
	 * not have a specified value, no object (NULL) is returned.
	 */
	static SVGObject* GetPresentationAttributeForDOM(HTML_Element *e, Markup::AttrType attr_type,
													 SVGObjectType object_type);

	/**
	 * Set the base value of the attribute.
	 */
	static OP_STATUS SetBaseObject(HTML_Element *e, Markup::AttrType attr_name, int ns_idx, BOOL is_special, SVGObject* obj);

#if 0
	/**
	 * Get the serial number for an attribute.
	 *
	 * Only valid in attributes where SVGAttr is stored
	 *
	 * This is used to increment the serial for each parsing from the
	 * doc module, and for other parts of the SVG module to see if a
	 * attribute has changed. If you intend to hold on to a SVGObject
	 * from an attribute, get the serial too. Then the serial can be
	 * used in checking if the object is still valid.
	 *
	 * @param e The relevant element.
	 *
	 * @param attr_name The attribute of the element to get the serial
	 * from.
	 * @param ns_idx Namespace index of the attribute.
	 */
	static UINT32 GetSerial(HTML_Element *e, int attr_name, int ns_idx);
#endif // 0

	/**
	 * Check if an element has a object at a specified
	 * attribute. Replaces the use of HTML_Element::HasAttr since
	 * there can be an attribute without any SVGObjects in it.
	 */
	static BOOL HasObject(HTML_Element* e, Markup::AttrType attr_name, int ns_idx, BOOL is_special = FALSE,
						  BOOL base = FALSE, SVGAttributeType anim_attribute_type = SVGATTRIBUTE_AUTO);

	/**
	 * Check if an element has a transform at a given
	 * attribute. HasObject() does not work in this case since the
	 * animated transform is stored in a seperate attribute,
	 * Markup::SVGA_ANIMATE_TRANSFORM.
	 */
	static BOOL HasTransform(HTML_Element* e, Markup::AttrType attr_name, int ns_idx);

	/**
	* Generic method for fetching an enum value and checking it's the right kind.
	*/
	static OP_STATUS GetEnumObject(HTML_Element *e, Markup::AttrType attr_type, SVGEnumType enum_type, SVGEnum** obj);

	static int GetEnumValue(HTML_Element *e, Markup::AttrType attr_type, SVGEnumType enum_type, int default_value);

	/**
	 * Fetch a proxy object
	 * @param e The HTML_Element to get attributes from
	 * @param type What attribute to get
	 * @param val The attribute value (output)
	 * @return OpStatus::OK if successful
	 */
	static OP_STATUS GetProxyObject(HTML_Element *e, Markup::AttrType type, SVGProxyObject **val);

	/**
	 * Generic method for fetching a length value.
	 * @param e The HTML_Element to get attributes from
	 * @param type What attribute to get
	 * @param val The attribute value (output)
	 * @return OpStatus::OK if successful
	 */
	static OP_STATUS GetLength(HTML_Element *e, Markup::AttrType type, SVGLengthObject **val, SVGLengthObject *def = NULL);

	/**
	 * Generic method for fetching a number value
	 */
	static OP_STATUS GetNumberObject(HTML_Element *e, Markup::AttrType type, SVGNumberObject **val);

	/**
	 * Convinience method for fetching a number
	 */
	static OP_STATUS GetNumber(HTML_Element *e, Markup::AttrType type, SVGNumber &val, SVGNumber def = 0);

	/**
	 * Generic method for fetching a string value
	 */
	static OP_STATUS GetString(HTML_Element *e, Markup::AttrType type, SVGString **val);

	/**
	 * Fetch a vector value. Only returns vectors with no errors in
	 * them.
	 */
	static void GetVector(HTML_Element *e, Markup::AttrType type, SVGVector*& list,
						  SVGAttributeField field_type = SVG_ATTRFIELD_DEFAULT);

	/**
	 * Fetch a vector value and return the parse status of the vector
	 */
	static OP_STATUS GetVectorWithStatus(HTML_Element *e, Markup::AttrType type, SVGVector*& list,
										 SVGAttributeField field_type = SVG_ATTRFIELD_DEFAULT);

	/**
	 * Generic method for fetching a computed transform value as a matrix
	 */
	static void GetMatrix(HTML_Element* e, Markup::AttrType type, SVGMatrix& matrix, SVGAttributeField field_type = SVG_ATTRFIELD_DEFAULT);

	/**
	 * Check if a transform attribute has the value ref(...) and in
	 * that case save the translatation matrix to 'matrix'. Returns
	 * TRUE if the transform attribute indeed was a ref transform,
	 * FALSE otherwise.
	 */
	static BOOL HasRefTransform(HTML_Element* e, Markup::AttrType type, SVGMatrix& matrix);

	/**
	 * Fetch an animation time.
	 */
	static OP_STATUS GetAnimationTime(HTML_Element* e, Markup::AttrType type,
									  SVG_ANIMATION_TIME &animation_time,
									  SVG_ANIMATION_TIME default_animation_time);

	/**
	 * Get a a specified path from an element
	 *
	 * @param e The element to extract information of
	 * @param type The attribute name that the path should be extracted from
	 * @return OpStatus::OK if the result is saved
	 */
	static OP_STATUS GetPath(HTML_Element *e, Markup::AttrType type, OpBpath **path)
	{
		SVGObject* obj;
		OP_ASSERT(path);
		OP_STATUS status = GetObject(e, type, SVGOBJECT_PATH, &obj);
		*path = static_cast<OpBpath*>(obj);
		return status;
	}

	/**
	 * Simplified GetPath that does not return an error code. If there
	 * was a path at attribute 'type' (animated or not), it is
	 * returned. Otherwise NULL is returned.
	 */
	static OpBpath* GetPath(HTML_Element *e, Markup::AttrType type)
	{
		SVGObject* obj;
		OpStatus::Ignore(GetObject(e, type, SVGOBJECT_PATH, &obj));
		return static_cast<OpBpath*>(obj);
	}

	/**
	 * Fetch a viewbox as a rectangle
	 */
	static OP_STATUS GetViewBox(HTML_Element *e, SVGRectObject **viewbox);

	/**
	 * Fetch a font size
	 */
	static OP_STATUS GetFontSize(HTML_Element *e, SVGFontSize &font_size);

	/**
	 * Fetch a font size
	 */
	static OP_STATUS GetFontSizeObject(HTML_Element *e, SVGFontSizeObject *&font_size_object);

	/**
	 * Fetch a copy of the repeat count value of an animation element
	 */
	static OP_STATUS GetRepeatCount(HTML_Element *e, SVGRepeatCount &repeat_count);

	/**
	 * Fetch a transform type
	 */
	static OP_STATUS GetTransformType(HTML_Element *e, SVGTransformType &type, SVGTransformType def = SVGTRANSFORM_TRANSLATE);

	/**
	 * @param root_url The documents root url. Needed to resolve relative URLs.
	 */
	static OP_STATUS GetXLinkHREF(URL root_url, HTML_Element *e, URL*& outurl, SVGAttributeField field_type = SVG_ATTRFIELD_DEFAULT, BOOL use_unreliable_cached = FALSE);

	/**
	 * Get the rotation attribute
	 */
	static OP_STATUS GetRotateObject(HTML_Element* e, SVGRotate **rotate);

	static OP_STATUS GetNavRefObject(HTML_Element *e, Markup::AttrType attr, SVGNavRef*& navref);

	static OP_STATUS GetVisibility(HTML_Element *e, SVGVisibilityType &type,
								   BOOL *cssprop = NULL, BOOL* is_inherit = NULL);
	static OP_STATUS GetCursor(HTML_Element *e, CursorType &cursor_type, BOOL* cssprop = NULL);
	static OP_STATUS GetDisplay(HTML_Element *e, SVGDisplayType &type, BOOL* cssprop = NULL);
	static OP_STATUS GetPreserveAspectRatio(HTML_Element *e, SVGAspectRatio*& ratio);

	static OP_STATUS GetUnits(HTML_Element *e, Markup::AttrType, SVGUnitsType &type, SVGUnitsType default_value);

	static OP_STATUS GetOrientObject(HTML_Element *e, SVGOrient*& orient);

	static SVGAnimationInterface* GetSVGAnimationInterface(HTML_Element *e);
	static SVGTimingInterface* GetSVGTimingInterface(HTML_Element *e);

	/** Get the SVGElementContext for an element. */
	static SVGElementContext* GetSVGElementContext(HTML_Element *e)
	{
		if (!e || e->GetNsType() != NS_SVG)
			return NULL;

		return static_cast<SVGElementContext*>(e->GetSVGContext());
	}
	/** Get the SVGElementContext for an element - assuming a
	 *	non-NULL element with a namespace of NS_SVG. */
	static SVGElementContext* GetSVGElementContext_Unsafe(HTML_Element *e)
	{
		return static_cast<SVGElementContext*>(e->GetSVGContext());
	}
	/**
	 * Get the SVGElementContext for an element. If missing, it will
	 * be created, but in that case the element *must* be an svg
	 * element which is not a Markup::SVGE_SVG or an animation element.
	 *
	 * If it returns NULL with create set to TRUE you can assume an OOM.
	 */
	static SVGElementContext* AssertSVGElementContext(HTML_Element *e);
	static SVGDocumentContext* GetSVGDocumentContext(HTML_Element *e);
	/**
	 * Get the SVGFontElement for an element. If create is set to TRUE one
	 * will be created if it's missing.
	 *
	 * If it returns NULL with create set to TRUE you can assume an OOM.
	 */
	static SVGFontElement* GetSVGFontElement(HTML_Element *e, BOOL create = TRUE);

	/**
	 * Mark an attribute as initialized
	 *
	 * @param attr_name must be an recognized attribute, not ATTR_XML.
	 */
	static void MarkAsInitialized(HTML_Element *element, Markup::AttrType attr_name, int ns_idx)
	{
		OP_ASSERT(attr_name != ATTR_XML);
		if (SVGAttribute* svg_attr = GetSVGAttr(element, attr_name, ns_idx))
			svg_attr->MarkAsInitialized();
	}

	/**
	 * Create the default attribute object for a specific attribute.
	 *
	 * @return OpStatus::OK if 'base_result' was allocated OK,
	 * otherwise an error code is returned and base_result is not assigned.
	 */
	static OP_STATUS CreateDefaultAttributeObject(HTML_Element* elm, Markup::AttrType attr_name, int ns_idx, BOOL is_special, SVGObject*& base_result);
private:
	static SVGAttribute *CreateAttribute(HTML_Element *element, Markup::AttrType attr_name, int ns_idx, BOOL is_special, SVGObject *base_object);
};

#endif // SVG_SUPPORT
#endif // _ATTR_VALUE_STORE_
