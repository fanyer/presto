/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef SVG_DOM

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domobj.h"
#include "modules/dom/src/domsvg/domsvgexception.h"

#include "modules/svg/svg_dominterfaces.h"

/* static */ void
DOM_SVGException::ConstructSVGExceptionObjectL(ES_Object *object, DOM_Runtime *runtime)
{
	DOM_Object::PutNumericConstantL(object, "SVG_WRONG_TYPE_ERR", SVGDOM::SVG_WRONG_TYPE_ERR, runtime);
	DOM_Object::PutNumericConstantL(object, "SVG_INVALID_VALUE_ERR", SVGDOM::SVG_INVALID_VALUE_ERR, runtime);
	DOM_Object::PutNumericConstantL(object, "SVG_MATRIX_NOT_INVERTABLE", SVGDOM::SVG_MATRIX_NOT_INVERTABLE, runtime);
}

#endif // SVG_DOM
