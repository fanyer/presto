/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#ifndef SVG_VECTOR_H
#define SVG_VECTOR_H

#ifdef SVG_SUPPORT

#include "modules/svg/src/SVGObject.h"
#include "modules/svg/svg_dominterfaces.h"

#include "modules/svg/src/animation/svganimationvaluecontext.h"
#include "modules/svg/src/animation/svganimationinterval.h"

enum SVGVectorSeparator
{
	SVGVECTORSEPARATOR_COMMA = 0,
	SVGVECTORSEPARATOR_SEMICOLON,
	SVGVECTORSEPARATOR_COMMA_OR_WSP
};

class SVGMatrix;
class SVGTransform;

// Suggestion: Move some data like separator and auto-deletion into
// the type instead.

class SVGVector : public SVGObject
{
public:
	/**
	 * Creates a vector, with a specified allocation step size.
	 */
	SVGVector(SVGObjectType vector_type = SVGOBJECT_UNKNOWN,
			  SVGVectorSeparator separator = SVGVECTORSEPARATOR_COMMA,
			  UINT32 stepsize = 10);

	virtual ~SVGVector();

	virtual BOOL IsEqual(const SVGObject& obj) const;
	virtual SVGObject *Clone() const;
	virtual OP_STATUS LowLevelGetStringRepresentation(TempBuffer* buffer) const;

	static void			AssignRef(SVGVector *&to, SVGVector *from);

	SVGObjectType VectorType() const { return static_cast<SVGObjectType>(m_vector_info.type); }
	SVGVectorSeparator Separator() const { return (SVGVectorSeparator)m_vector_info.separator; }
	void SetSeparator(SVGVectorSeparator separator) { m_vector_info.separator = separator; }

	BOOL IsNone() const { return m_vector_info.is_none; }
	void SetIsNone(BOOL is_none);

	BOOL IsRefTransform() const { return m_vector_info.is_ref_transform; }
	void SetIsRefTransform() { m_vector_info.is_ref_transform = 1; }

	BOOL IsUsedByDOM() const { return m_vector_info.used_by_dom; }
	void SetUsedByDOM(BOOL used_by_dom);

	/** Functions used by the animation engine */
	OP_STATUS Interpolate(SVGAnimationValueContext &context,
						  const SVGVector& v1, const SVGVector& v2,
						  SVG_ANIMATION_INTERVAL_POSITION interval_position);
	static float CalculateDistance(const SVGVector& v1, const SVGVector& v2,
								   SVGAnimationValueContext &context);
	OP_STATUS Copy(SVGAnimationValueContext &context, SVGVector &v1);
	virtual BOOL InitializeAnimationValue(SVGAnimationValue &animation_value);
	/* End animation */

	/**
	 * Clear vector, returning the vector to an empty state
	 */
	void Clear();

	/**
	 * Sets the allocation step size
	 */
	void SetAllocationStepSize(UINT32 step) { m_internal_vector.SetAllocationStepSize(step); }

	/**
	 * Replace the item at index with new item
	 */
	OP_STATUS Replace(UINT32 idx, SVGObject* item);

	/**
	 * Insert and add an item of type T at index idx. The index must
	 * be inside the current vector, or added to the end. This does
	 * not replace, the vector will grow.  The reference counter of
	 * item is automatically increased by one.
	 */
	OP_STATUS Insert(UINT32 idx, SVGObject* item);

	/**
	 * Adds the item to the end of the vector. The reference counter
	 * of item is automatically increased by one.
	 */
	OP_STATUS Append(SVGObject* item);

	/**
	 * Removes item if found in vector
	 */
	OP_STATUS RemoveByItem(SVGObject* item);

	/**
	 * Removes count items starting at index idx and returns the pointer to the first item.
	 */
	void Remove(UINT32 idx, UINT32 count = 1);

	/**
	 * Finds the index of a given item
	 */
	INT32 Find(SVGObject* item) const { return m_internal_vector.Find(item); }

	/**
	 * Returns the item at index idx.
	 */
	SVGObject* Get(UINT32 idx) const { return static_cast<SVGObject*>(m_internal_vector.Get(idx)); }

	/**
	 * Returns the number of items in the vector.
	 */
	UINT32 GetCount() const { return m_internal_vector.GetCount(); }

	OP_STATUS Consolidate();

	void GetMatrix(SVGMatrix& matrix) const;
	void GetTransform(SVGTransform &transform) const;

	OP_STATUS Initialize(SVGObject* item);

protected:
	OpVector<SVGObject> m_internal_vector;

	union {
		struct {
			unsigned int		type:8;			// SVGObjectType
			unsigned int		separator:2;
			unsigned int		is_none:1;		// only for dash-arrays and transform lists
			unsigned int		is_ref_transform:1; // only for transform lists
			unsigned int		used_by_dom:1;
			/* 13 bits */
		} m_vector_info;
		unsigned short m_vector_info_init;
	};
};

#endif // SVG_SUPPORT
#endif // SVG_VECTOR_H
