/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#ifdef SVG_SUPPORT

#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/SVGVector.h"
#include "modules/svg/src/SVGMatrix.h"
#include "modules/svg/src/SVGTransform.h"
#include "modules/svg/src/animation/svganimationvalue.h"

#include "modules/util/tempbuf.h"

SVGVector::SVGVector(SVGObjectType vector_type /* = SVGOBJECT_UNKNOWN */,
					 SVGVectorSeparator separator /* = SVGVECTORSEPARATOR_COMMA */,
					 UINT32 stepsize /* = 10 */)
		: SVGObject(SVGOBJECT_VECTOR),
		  m_internal_vector(stepsize),
		  m_vector_info_init(0)
{
	SetSeparator(separator);
	m_vector_info.type = vector_type;
}

/* virtual */
SVGVector::~SVGVector()
{
	Clear();
}

/* virtual */ BOOL
SVGVector::IsEqual(const SVGObject& obj) const
{
	if (obj.Type() == SVGOBJECT_VECTOR)
	{
		const SVGVector& other = static_cast<const SVGVector&>(obj);

		if (VectorType() == SVGOBJECT_TRANSFORM)
		{
			/* The result is the important thing, not how we get
			 * there. So translate(100,100) translate(10,10) should be
			 * the same as translate(10,10) translate(100,100)
			 */
			SVGMatrix m1;
			SVGMatrix m2;
			GetMatrix(m1);
			other.GetMatrix(m2);
			return m1 == m2;
		}
		else
		{
			// compare item by item
			if (other.GetCount() == GetCount())
			{
				for(UINT32 i=0;i<GetCount();++i)
				{
					SVGObject* obj1 = Get(i);
					SVGObject* obj2 = other.Get(i);
					if (!obj1 || !obj2 || obj1->Type() != obj2->Type())
					{
						return FALSE;
					}
					else
					{
						if (!obj1->IsEqual(*obj2))
							return FALSE;
					}
				}
				return TRUE;
			}
		}
	}
	return FALSE;
}

void
SVGVector::GetMatrix(SVGMatrix& accumulator) const
{
	accumulator.LoadIdentity();

	if (VectorType() != SVGOBJECT_TRANSFORM)
		return;

	for (int i=GetCount()-1;i>=0;i--)
	{
		SVGObject* obj = Get(i);
		OP_ASSERT(obj->Type() == SVGOBJECT_TRANSFORM);

		SVGTransform* tform = static_cast<SVGTransform*>(obj);
		SVGMatrix matrix;
		tform->GetMatrix(matrix);
		accumulator.Multiply(matrix);
	}
}

void
SVGVector::GetTransform(SVGTransform& accumulator) const
{
	if (GetCount() == 1 && VectorType() == SVGOBJECT_TRANSFORM)
	{
		accumulator.Copy(*static_cast<SVGTransform*>(Get(0)));
	}
	else
	{
		SVGMatrix matrix;
		GetMatrix(matrix);
		accumulator.SetMatrix(matrix);
	}
}

OP_STATUS
SVGVector::RemoveByItem(SVGObject* item)
{
	INT32 idx = m_internal_vector.Find(item);
	if (idx == -1)
		return OpStatus::ERR;
	Remove(idx);
	return OpStatus::OK;
}

void
SVGVector::Clear()
{
	for(UINT32 i=0; i<GetCount(); ++i)
		SVGObject::DecRef(Get(i));
	m_internal_vector.Clear();
}

void
SVGVector::Remove(UINT32 idx, UINT32 count /* = 1 */)
{
	OP_ASSERT(idx + count <= GetCount());

	for(UINT32 i=idx; i<(count+idx); ++i)
		SVGObject::DecRef(Get(i));

	m_internal_vector.Remove(idx, count);
}

OP_STATUS
SVGVector::Initialize(SVGObject* item)
{
	if (GetCount() > 0)
	{
		Remove(0, GetCount());
	}
	return Append(item);
}

#ifdef SVG_DOM
OP_STATUS
SVGVector::Consolidate()
{
	if (GetCount() > 0)
	{
		SVGMatrix matrix;
		GetMatrix(matrix);
		Clear();

		SVGTransform* tfm = OP_NEW(SVGTransform, ());
		if(!tfm)
			return OpStatus::ERR_NO_MEMORY;

		tfm->SetMatrix(matrix);

		OP_STATUS err = Append(tfm);
		if(OpStatus::IsError(err))
		{
			OP_DELETE(tfm);
			return err;
		}
	}

	return OpStatus::OK;
}
#endif // SVG_DOM

OP_STATUS
SVGVector::Replace(UINT32 idx, SVGObject* item)
{
	if (item && (item->Type() == (SVGObjectType)m_vector_info.type ||
				 m_vector_info.type == SVGOBJECT_UNKNOWN))
	{
		SVGObject::DecRef(m_internal_vector.Get(idx));

		// If replace failes, what is the state of the previous item?
		RETURN_IF_ERROR(m_internal_vector.Replace(idx, item));
		SVGObject::IncRef(item);
		return OpStatus::OK;
	}
	else
	{
		OP_ASSERT(!"Wrong type!");
		return OpStatus::ERR;
	}
}

OP_STATUS
SVGVector::Insert(UINT32 idx, SVGObject* item)
{
	if (item && (item->Type() == (SVGObjectType)m_vector_info.type ||
				 m_vector_info.type == SVGOBJECT_UNKNOWN))
	{
		RETURN_IF_ERROR(m_internal_vector.Insert(idx, item));
		SVGObject::IncRef(item);
		return OpStatus::OK;
	}
	else
	{
		OP_ASSERT(!"Wrong type!");
		return OpStatus::ERR;
	}
}

OP_STATUS
SVGVector::Append(SVGObject* item)
{
	if (item && (item->Type() == (SVGObjectType)m_vector_info.type ||
				 m_vector_info.type == SVGOBJECT_UNKNOWN))
	{
		RETURN_IF_ERROR(m_internal_vector.Add(item));
		SVGObject::IncRef(item);
		return OpStatus::OK;
	}
	else
	{
		OP_ASSERT(!"Wrong type!");
		return OpStatus::ERR;
	}
}

void SVGVector::SetIsNone(BOOL is_none)
{
	if(is_none)
	{
		Clear();
	}

	m_vector_info.is_none = is_none ? 1 : 0;
}

SVGObject* SVGVector::Clone() const
{
	SVGVector* new_vec = OP_NEW(SVGVector, (VectorType()));
	if (!new_vec)
		return NULL;

	new_vec->CopyFlags(*this);

	for(UINT32 i=0;i<GetCount();++i)
	{
		SVGObject* new_obj = NULL;
		SVGObject* old_obj = Get(i);
		if (old_obj != NULL)
		{
			new_obj = old_obj->Clone();
			if (!new_obj)
			{
				OP_DELETE(new_vec);
				return NULL;
			}
			OP_STATUS status = new_vec->Append(new_obj);
			if (OpStatus::IsError(status))
			{
				OP_DELETE(new_vec);
				return NULL;
			}
		}
	}

	new_vec->m_vector_info = m_vector_info;
	return new_vec;
}

OP_STATUS SVGVector::LowLevelGetStringRepresentation(TempBuffer* buffer) const
{
	UINT32 i = 0;
	if(IsNone())
	{
		return buffer->Append("none");
	}

	if (GetCount() == 0)
	{
		RETURN_IF_ERROR(buffer->Append(""));
	}
	else
		do
		{
			SVGObject* obj = Get(i++);
			if(obj)
			{
				RETURN_IF_ERROR(obj->GetStringRepresentation(buffer));
			}

			if(i < GetCount())
			{
				const char* separator = ",";
				if(Separator() == SVGVECTORSEPARATOR_SEMICOLON)
					separator = ";";
				else if(Separator() == SVGVECTORSEPARATOR_COMMA_OR_WSP)
					separator = " ";

				RETURN_IF_ERROR(buffer->Append(separator, 1)); 
			}

		} while(i < GetCount());

	return OpStatus::OK;
}

OP_STATUS
SVGVector::Interpolate(SVGAnimationValueContext &context,
					   const SVGVector& v1, const SVGVector& v2,
					   SVG_ANIMATION_INTERVAL_POSITION interval_position)
{
	BOOL append = (GetCount() == 0);
	if (v1.VectorType() == VectorType() && v2.VectorType() == VectorType() && VectorType() != SVGOBJECT_UNKNOWN)
	{
		if (v1.GetCount() == v2.GetCount() && (append || v1.GetCount() == GetCount()))
		{
			SVGAnimationValue from_value;
			SVGAnimationValue to_value;
			SVGAnimationValue target_value;

			for (UINT32 idx = 0; idx < v1.GetCount(); idx++)
			{
				SVGObject* o1 = v1.Get(idx);
				SVGObject* o2 = v2.Get(idx);
				SVGObject *target_object = NULL;

				if (o1 != NULL && o2 != NULL)
				{
					SVGAnimationValue::Initialize(from_value, o1, context);
					SVGAnimationValue::Initialize(to_value, o2, context);

					if (append)
					{
						target_object = o1->Clone();
						if (!target_object || OpStatus::IsMemoryError(Append(target_object)))
							return OpStatus::ERR_NO_MEMORY;
					}
					else
					{
						target_object = Get(idx);
					}

					SVGAnimationValue::Initialize(target_value, target_object, context);
					OP_STATUS interpolate_status = SVGAnimationValue::Interpolate(context,
																				  interval_position,
																				  from_value,
																				  to_value,
																				  SVGAnimationValue::EXTRA_OPERATION_NOOP,
																				  target_value);
					if (OpStatus::IsError(interpolate_status))
						return interpolate_status;
				}
			}
			return OpStatus::OK;
		}
	}
	return OpStatus::ERR_NOT_SUPPORTED;
}

OP_STATUS
SVGVector::Copy(SVGAnimationValueContext &context, SVGVector &v1)
{
	SVGObject::CopyFlags(v1);

	// This clears the vector of objects if necessary
	SetIsNone(v1.IsNone());

	if (GetCount() == v1.GetCount())
	{
		SVGAnimationValue dest_value;
		SVGAnimationValue src_value;
		for (UINT32 idx = 0; idx < GetCount(); idx++)
		{
			SVGObject *dest_object = Get(idx);
			SVGObject *src_object = v1.Get(idx);

			SVGAnimationValue::Initialize(dest_value, dest_object, context);
			SVGAnimationValue::Initialize(src_value, src_object, context);

			OP_STATUS status = SVGAnimationValue::Assign(context, src_value, dest_value);
			if (OpStatus::IsMemoryError(status))
			{
				return OpStatus::ERR_NO_MEMORY;
			}
		}
	}
	else
	{
		Clear();
		for (UINT32 idx = 0; idx < v1.GetCount(); idx++)
		{
			SVGObject* dest_object = v1.Get(idx)->Clone();
			if (!dest_object || OpStatus::IsMemoryError(Append(dest_object)))
			{
				OP_DELETE(dest_object);
				return OpStatus::ERR_NO_MEMORY;
			}
		}
	}

	m_vector_info = v1.m_vector_info;
	return OpStatus::OK;
}

/* virtual */ BOOL
SVGVector::InitializeAnimationValue(SVGAnimationValue &animation_value)
{
	animation_value.reference_type = SVGAnimationValue::REFERENCE_SVG_VECTOR;
	animation_value.reference.svg_vector = this;
	return TRUE;
}

/* static */ float
SVGVector::CalculateDistance(const SVGVector& v1, const SVGVector& v2,
							 SVGAnimationValueContext &context)
{
	if (v1.VectorType() == v2.VectorType() &&
		v1.VectorType() != SVGOBJECT_UNKNOWN &&
		v1.GetCount() == v2.GetCount())
	{
		SVGAnimationValue from_value;
		SVGAnimationValue to_value;

		float sum = 0.0;

		for (UINT32 idx = 0; idx < v1.GetCount(); idx++)
		{
			SVGObject* o1 = v1.Get(idx);
			SVGObject* o2 = v2.Get(idx);

			SVGAnimationValue::Initialize(from_value, o1, context);
			SVGAnimationValue::Initialize(to_value, o2, context);
			sum += SVGAnimationValue::CalculateDistance(context, from_value, to_value);
		}

		return sum / v1.GetCount();
	}
	else
	{
		return 0.0;
	}
}

/* static */ void
SVGVector::AssignRef(SVGVector *&to, SVGVector *from)
{
	if (to != from)
	{
		DecRef(to);
		IncRef(to = from);
	}
}

#endif // SVG_SUPPORT
