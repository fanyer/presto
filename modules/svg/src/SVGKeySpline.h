/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#ifndef SVG_KEYSPLINE
#define SVG_KEYSPLINE

#ifdef SVG_SUPPORT

#include "modules/svg/src/SVGObject.h"

class SVGKeySpline : public SVGObject
{
public:
	SVGNumber m_v[4];

	SVGKeySpline(SVGNumber v1, SVGNumber v2, SVGNumber v3, SVGNumber v4)
			: SVGObject(SVGOBJECT_KEYSPLINE)
	{
		m_v[0] = v1;
		m_v[1] = v2;
		m_v[2] = v3;
		m_v[3] = v4;
	}

	virtual BOOL IsEqual(const SVGObject &other) const {
		if (other.Type() == SVGOBJECT_KEYSPLINE)
		{
			const SVGKeySpline &other_keyspline = static_cast<const SVGKeySpline &>(other);
			for (int i = 0; i < 4; i++)
				if (other_keyspline.m_v[i] != m_v[i])
					return FALSE;
			return TRUE;
		}
		else
		{
			return FALSE;
		}
	}

	virtual SVGObject *Clone() const { return OP_NEW(SVGKeySpline, (m_v[0], m_v[1], m_v[2], m_v[3])); }

	virtual OP_STATUS LowLevelGetStringRepresentation(TempBuffer* buffer) const {
		for (int i = 0; i < 4; i++)
		{
			if (i > 0)
				RETURN_IF_ERROR(buffer->Append(' '));
			RETURN_IF_ERROR(buffer->AppendDoubleToPrecision(m_v[i].GetFloatValue()));
		}
		return OpStatus::OK;
	}
};

#endif // SVG_SUPPORT
#endif // SVG_KEYSPLINE
