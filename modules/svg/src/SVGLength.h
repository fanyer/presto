/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#ifndef SVG_LENGTH_H
#define SVG_LENGTH_H

#ifdef SVG_SUPPORT

#include "modules/style/css_value_types.h"
#include "modules/svg/svg_external_types.h"
#include "modules/svg/src/SVGObject.h"

struct SVGValueContext;

/**
 * This class wraps the SVGLength class
 */
class SVGLengthObject : public SVGObject
{
public:
	SVGLengthObject(SVGNumber number = 0, int unit = CSS_NUMBER);
	SVGLengthObject(const SVGLength* length);
	virtual ~SVGLengthObject() {}

	virtual OP_STATUS LowLevelGetStringRepresentation(TempBuffer* buffer) const;
	virtual SVGObject *Clone() const;
	virtual BOOL IsEqual(const SVGObject& other) const;

	// Animation support for lengths

	virtual BOOL InitializeAnimationValue(SVGAnimationValue &animation_value);

	/**
	 * Copy a length object
	 */
	void Copy(const SVGLengthObject& v1);

	int GetUnit() const { return m_length.GetUnit(); }
	SVGNumber GetScalar() const { return m_length.GetScalar(); }

	void SetScalar(SVGNumber scalar) { m_length.SetScalar(scalar); }
	void SetUnit(int cssunit) { m_length.SetUnit(cssunit); }

	SVGLength& GetLength() { return m_length; }
	const SVGLength& GetLength() const { return m_length; }

	void ConvertToUnit(int css_unit, SVGLength::LengthOrientation type, const SVGValueContext& vcxt);

private:
	SVGLength m_length;
};

#endif // SVG_SUPPORT
#endif // SVG_LENGTH_H
