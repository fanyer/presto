/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#ifndef SVG_OBJECT_H
#define SVG_OBJECT_H

#ifdef SVG_SUPPORT

#include "modules/svg/svg_dominterfaces.h"
#include "modules/svg/svg_external_types.h"
#include "modules/svg/svg_number.h"
#include "modules/svg/SVGContext.h"

#include "modules/logdoc/complex_attr.h"
#include "modules/util/tempbuf.h"

#define SVG_ASSERT_ON_UNIMPLEMENTED_ANIM 1

/**
 * If changed are made to this enumeration, update
 * SVG_object_type_strings in SVGManagerImpl.cpp to match.
 * Must fit in the 'type' bitfield in SVGObject
 */
enum SVGObjectType
{
	SVGOBJECT_NUMBER = 0,
	SVGOBJECT_POINT,
	SVGOBJECT_LENGTH,
	SVGOBJECT_ENUM,
	SVGOBJECT_COLOR,
	SVGOBJECT_PATH,
	SVGOBJECT_VECTOR,
	SVGOBJECT_RECT,
	SVGOBJECT_PAINT,
	SVGOBJECT_TRANSFORM,
	SVGOBJECT_MATRIX,
	SVGOBJECT_COORDINATE_PAIR,
	SVGOBJECT_KEYSPLINE,
	SVGOBJECT_STRING,
	SVGOBJECT_FONTSIZE,
	SVGOBJECT_TIME,
	SVGOBJECT_ROTATE,
	SVGOBJECT_ANIMATION_TIME,
	SVGOBJECT_ASPECT_RATIO,
	SVGOBJECT_URL,
#ifdef SVG_SUPPORT_FILTERS
	SVGOBJECT_ENABLE_BACKGROUND,
#endif // SVG_SUPPORT_FILTERS
	SVGOBJECT_BASELINE_SHIFT,
	SVGOBJECT_ANGLE,
	SVGOBJECT_ORIENT,
	SVGOBJECT_PATHSEG,
	SVGOBJECT_NAVREF,
	SVGOBJECT_REPEAT_COUNT,
	SVGOBJECT_PROXY,
	SVGOBJECT_CLASSOBJECT,
	SVGOBJECT_UNKNOWN
};

typedef UINT16 SVGObjectFlagStore;

/**
 * These must fit in SVGObject::m_packed.flags
 */
enum SVGObjectFlag
{
	SVGOBJECTFLAG_ERROR				= 1L << 0,	///< Set if the value of object is in error
	SVGOBJECTFLAG_INHERIT			= 1L << 1,	///< Set if the value of object is 'inherit'
	SVGOBJECTFLAG_IS_CSSPROP		= 1L << 2,	///< Only intended to be used when animating CSS properties
	SVGOBJECTFLAG_UNINITIALIZED     = 1L << 3,  ///< Set if the object is uninitialized and not meant for reading
	SVGOBJECTFLAG_SENTINEL          = 1L << 4   ///< Must be last (and greatest). Has no storage in bitfield. Only used for detecting out-of-bound access
};

class SVGObject;
class TempBuffer;
class SVGAnimationValue;

/**
 * Base object to all object the are saved in attributes of the
 * document tree.
 *
 * The error is set when parsing, but read when rendering. This is to
 * allow rendering up to an error.
 */
class SVGObject
{
public:
	SVGObject(SVGObjectType type);
	virtual ~SVGObject() { OP_ASSERT(m_refcount == 0 || m_object_info_packed.protect); }

	SVGObjectType Type() const { return static_cast<SVGObjectType>(m_object_info_packed.type); }
	BOOL HasError() const { return (m_object_info_packed.flags & SVGOBJECTFLAG_ERROR) != 0; }
	void SetStatus(OP_STATUS status) { if(OpStatus::IsError(status))
										(m_object_info_packed.flags |= SVGOBJECTFLAG_ERROR); }

	/**
	 * Returns if two objects can be considered equal.
	 *
	 * This method should only be used in optimizations, and it rather
	 * returns FALSE for two values that can in some way be considered
	 * equal than returns TRUE for attributes that would give divering
	 * drawing results if the object was part of a drawing operation.
	 *
	 * This method is used to avoid unnecessary repaints on attribute
	 * changes, both from animation and from DOM.
	 */
	virtual BOOL IsEqual(const SVGObject& other) const = 0;

	virtual BOOL InitializeAnimationValue(SVGAnimationValue &animation_value) { return FALSE; }

	OP_STATUS GetStringRepresentation(TempBuffer* buffer) const;

	/**
	 * This method is used when cloning a document tree and must be
	 * implemented for objects stored in attributes.
	 */
	virtual SVGObject *Clone() const = 0;
	virtual OP_STATUS LowLevelGetStringRepresentation(TempBuffer* buffer) const = 0;

	BOOL IsThisType(SVGObject const* v1) const { return (v1 && v1->m_object_info_packed.type == m_object_info_packed.type); }

	void UnsetFlag(SVGObjectFlag flag) { OP_ASSERT(flag < SVGOBJECTFLAG_SENTINEL); m_object_info_packed.flags &= ~flag; }
	void SetFlag(SVGObjectFlag flag) { OP_ASSERT(flag < SVGOBJECTFLAG_SENTINEL); m_object_info_packed.flags |= flag; }
	BOOL Flag(SVGObjectFlag flag) const { OP_ASSERT(flag < SVGOBJECTFLAG_SENTINEL); return (m_object_info_packed.flags & flag) != 0; }

	static SVGObject*	IncRef(SVGObject* obj);
	static void			DecRef(SVGObject* obj);
	static SVGObject*	Protect(SVGObject* obj);

	static void			AssignRef(SVGObject *&to, SVGObject *from);

	/**
	 * Create the default value for a given type
	 *
	 * @param obj The object is saved here
	 * @param type The type of the object to create.
	 * @return OpStatus::OK if a object could be created.
	 * OpStatus::TYPE_ERROR if the type cannot be created.
	 */
	static OP_STATUS Make(SVGObject*& obj, SVGObjectType type);

protected:
	void CopyFlags(const SVGObject &other) { m_object_info_packed_init = (m_object_info_packed_init & SVGOBJECTFLAG_IS_CSSPROP) | other.m_object_info_packed_init; }

private:
	SVGObject(const SVGObject& X);
	void operator=(const SVGObject& X);

	int m_refcount;

	union {
		struct {
			unsigned int flags:7;
			unsigned int type:8;	///< SVGObjectType
			unsigned int protect:1;
			/* 32 bits */
		} m_object_info_packed;
		unsigned int m_object_info_packed_init;
	};
};

#endif // SVG_SUPPORT
#endif // SVG_OBJECT_H
