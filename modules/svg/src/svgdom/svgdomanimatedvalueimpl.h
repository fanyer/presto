/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef SVG_DOM_ANIMATED_VALUE_IMPL_H
#define SVG_DOM_ANIMATED_VALUE_IMPL_H

# ifdef SVG_FULL_11

#include "modules/svg/svg_dominterfaces.h"
#include "modules/svg/src/SVGObject.h"

class SVGDOMAnimatedValueImpl : public SVGDOMAnimatedValue
{
public:
	static OP_STATUS Make(SVGDOMAnimatedValueImpl*& animated_value,
						  SVGObject* base_obj, SVGObject* anim_obj,
						  SVGDOMItemType itemtype, SVGDOMItemType sub_itemtype = SVG_DOM_ITEMTYPE_UNKNOWN);
	virtual ~SVGDOMAnimatedValueImpl();

	virtual OP_BOOLEAN GetPrimitiveValue(SVGDOMPrimitiveValue& value, Field field);

	virtual BOOL IsSetByNumber();
	virtual OP_BOOLEAN SetNumber(double number);

	virtual BOOL IsSetByString();
	virtual OP_BOOLEAN SetString(const uni_char *str, LogicalDocument* logdoc);

	virtual const char* GetDOMName();

private:
	SVGDOMAnimatedValueImpl(SVGObject* base, SVGObject* anim, const char* name,
							SVGDOMItemType itemtype, SVGDOMItemType sub_itemtype);

	/**
	 * May only return success or OpStatus::ERR_NO_MEMORY. If the
	 * object can't be wrapped write NULL to value and return
	 * OpBoolean::IS_FALSE.
	 */
	static OP_BOOLEAN LowGetValue(SVGDOMPrimitiveValue &value, SVGDOMItemType type,
								  SVGDOMItemType subtype, SVGObject* source);
	static const char* LowGetDOMName(SVGDOMItemType type, SVGDOMItemType subtype);

	SVGDOMItemType m_itemtype;
	SVGDOMItemType m_sub_itemtype;

	SVGObject *m_base;
	SVGObject *m_anim;
	const char* m_name;
};

class SVGVector;
class SVGTransform;

class SVGDOMAnimatedValueTransformListImpl : public SVGDOMAnimatedValue
{
public:
	SVGDOMAnimatedValueTransformListImpl(SVGVector* base_vector, SVGTransform* anim_transform,
									BOOL additive);
	virtual ~SVGDOMAnimatedValueTransformListImpl();

	virtual OP_BOOLEAN GetPrimitiveValue(SVGDOMPrimitiveValue &value, Field field);

	virtual BOOL IsSetByNumber() { return FALSE; }
	virtual OP_BOOLEAN SetNumber(double number) { return OpStatus::ERR; }

	virtual BOOL IsSetByString() { return FALSE; }
	virtual OP_BOOLEAN SetString(const uni_char *str, LogicalDocument* logdoc) { return OpStatus::ERR; }

	virtual const char* GetDOMName();
private:
	SVGVector *m_base;
	SVGTransform *m_anim;
	BOOL m_additive;
};

# endif // SVG_FULL_11
#endif // !SVG_DOM_ANIMATED_VALUE_IMPL_H
