/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_CSS_TRANSFORM_VALUE_H
#define DOM_CSS_TRANSFORM_VALUE_H

#ifdef CSS_TRANSFORMS

class DOM_CSSStyleDeclaration;

#include "modules/display/vis_dev_transform.h"

class DOM_CSSMatrix : public DOM_Object
{
public:
    ~DOM_CSSMatrix() {}
    static OP_STATUS Make(DOM_CSSMatrix*& matrix, const AffineTransform &t, DOM_CSSStyleDeclaration *s);

    virtual void GCTrace();
    virtual BOOL IsA(int type) { return type == DOM_TYPE_CSS_MATRIX || DOM_Object::IsA(type); }

    virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
    virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);

    DOM_DECLARE_FUNCTION(setMatrixValue);
    DOM_DECLARE_FUNCTION(multiply);
    DOM_DECLARE_FUNCTION(inverse);
    DOM_DECLARE_FUNCTION(translate);
    DOM_DECLARE_FUNCTION(scale);
    DOM_DECLARE_FUNCTION(rotate);

    enum { FUNCTIONS_ARRAY_SIZE = 7 };

private:
    DOM_CSSMatrix(const AffineTransform &t, DOM_CSSStyleDeclaration *s) : transform(t), style_declaration(s) {}

    AffineTransform transform;
    DOM_CSSStyleDeclaration *style_declaration;
};

#endif // !CSS_TRANSFORMS
#endif // !DOM_CSS_TRANSFORM_VALUE_H
