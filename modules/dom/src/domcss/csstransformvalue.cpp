/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef CSS_TRANSFORMS

#ifndef M_PI
#define M_PI			3.14159265358979323846	/* pi */
#endif

#include "modules/dom/src/domcss/cssstyledeclaration.h"
#include "modules/dom/src/domcss/csstransformvalue.h"
#include "modules/style/css_dom.h"

/* static */ OP_STATUS
DOM_CSSMatrix::Make(DOM_CSSMatrix*& matrix, const AffineTransform &t, DOM_CSSStyleDeclaration* s)
{
	DOM_Runtime *runtime = s->GetRuntime();

	return DOMSetObjectRuntime(matrix = OP_NEW(DOM_CSSMatrix, (t, s)),
	                           runtime, runtime->GetPrototype(DOM_Runtime::CSSMATRIX_PROTOTYPE), "CSSMatrix");
}

/* virtual */ void
DOM_CSSMatrix::GCTrace()
{
	GCMark(style_declaration);
}

/* static */ int
DOM_CSSMatrix::setMatrixValue(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(matrix, DOM_TYPE_CSS_MATRIX, DOM_CSSMatrix);
	DOM_CHECK_ARGUMENTS("s");

	CSS_DOMStyleDeclaration *style = matrix->style_declaration->GetStyleDeclaration();

	OP_STATUS status = style->GetAffineTransform(matrix->transform, argv[0].value.string);

	if (status == OpStatus::ERR_NO_MEMORY)
		return ES_NO_MEMORY;
	else if (status == OpStatus::ERR)
		return DOM_CALL_DOMEXCEPTION(SYNTAX_ERR);
	else
		return ES_FAILED;
}

/* static */ int
DOM_CSSMatrix::multiply(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(matrix, DOM_TYPE_CSS_MATRIX, DOM_CSSMatrix);
	DOM_CHECK_ARGUMENTS("o");
	DOM_ARGUMENT_OBJECT(passed_matrix, 0, DOM_TYPE_CSS_MATRIX, DOM_CSSMatrix);

	AffineTransform transform;

	transform = matrix->transform;
	transform.PostMultiply(passed_matrix->transform);

	DOM_CSSMatrix *result_matrix;
	if (OpStatus::IsMemoryError(DOM_CSSMatrix::Make(result_matrix, transform, matrix->style_declaration)))
		return ES_NO_MEMORY;

	DOMSetObject(return_value, result_matrix);
	return ES_VALUE;
}

/* static */ int
DOM_CSSMatrix::inverse(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(matrix, DOM_TYPE_CSS_MATRIX, DOM_CSSMatrix);

	AffineTransform transform = matrix->transform;
	if (!transform.Invert())
		return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);

	DOM_CSSMatrix *inverse_matrix;
	if (OpStatus::IsMemoryError(DOM_CSSMatrix::Make(inverse_matrix, transform, matrix->style_declaration)))
		return ES_NO_MEMORY;

	DOMSetObject(return_value, inverse_matrix);
	return ES_VALUE;
}

/* static */ int
DOM_CSSMatrix::translate(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(matrix, DOM_TYPE_CSS_MATRIX, DOM_CSSMatrix);
	DOM_CHECK_ARGUMENTS("nn");

	float tx = static_cast<float>(argv[0].value.number);
	float ty = static_cast<float>(argv[1].value.number);

	AffineTransform transform = matrix->transform;
	transform.PostTranslate(tx, ty);

	DOM_CSSMatrix *result_matrix;
	if (OpStatus::IsMemoryError(DOM_CSSMatrix::Make(result_matrix, transform, matrix->style_declaration)))
		return ES_NO_MEMORY;

	DOMSetObject(return_value, result_matrix);
	return ES_VALUE;
}

/* static */ int
DOM_CSSMatrix::scale(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(matrix, DOM_TYPE_CSS_MATRIX, DOM_CSSMatrix);
	DOM_CHECK_ARGUMENTS("n|n");

	float sx = static_cast<float>(argv[0].value.number);
	float sy;
	if (argc == 2)
		sy = static_cast<float>(argv[1].value.number);
	else
		sy = sx;

	AffineTransform tmp;
	tmp.LoadScale(sx, sy);

	AffineTransform transform = matrix->transform;
	transform.PostMultiply(tmp);

	DOM_CSSMatrix *result_matrix;
	if (OpStatus::IsMemoryError(DOM_CSSMatrix::Make(result_matrix, transform, matrix->style_declaration)))
		return ES_NO_MEMORY;

	DOMSetObject(return_value, result_matrix);
	return ES_VALUE;
}

/* static */ int
DOM_CSSMatrix::rotate(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(matrix, DOM_TYPE_CSS_MATRIX, DOM_CSSMatrix);
	DOM_CHECK_ARGUMENTS("n");

	double s = argv[0].value.number;

	AffineTransform tmp;

	// Input is in degrees, AffineTransform wants radians.
	float radians = static_cast<float>(s * M_PI / 180.0);
	tmp.LoadRotate(radians);

	AffineTransform transform = matrix->transform;
	transform.PostMultiply(tmp);

	DOM_CSSMatrix *result_matrix;
	if (OpStatus::IsMemoryError(DOM_CSSMatrix::Make(result_matrix, transform, matrix->style_declaration)))
		return ES_NO_MEMORY;

	DOMSetObject(return_value, result_matrix);
	return ES_VALUE;
}

static int IndexFromAtom(OpAtom atom)
{
	switch(atom)
	{
	default: OP_ASSERT(!"Not reached");
		// Fall through
	case OP_ATOM_a: return 0;
	case OP_ATOM_c: return 1;
	case OP_ATOM_e: return 2;
	case OP_ATOM_b: return 3;
	case OP_ATOM_d: return 4;
	case OP_ATOM_f: return 5;
	}
}

/* virtual */ ES_GetState
DOM_CSSMatrix::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch(property_name)
	{
	case OP_ATOM_a:
	case OP_ATOM_b:
	case OP_ATOM_c:
	case OP_ATOM_d:
	case OP_ATOM_e:
	case OP_ATOM_f:
		if (value)
			DOMSetNumber(value, transform[IndexFromAtom(property_name)]);
		return GET_SUCCESS;
	default:
		return DOM_Object::GetName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_PutState
DOM_CSSMatrix::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch(property_name)
	{
	case OP_ATOM_a:
	case OP_ATOM_b:
	case OP_ATOM_c:
	case OP_ATOM_d:
	case OP_ATOM_e:
	case OP_ATOM_f:
		if (value->type != VALUE_NUMBER)
			return PUT_NEEDS_NUMBER;

		transform[IndexFromAtom(property_name)] = static_cast<float>(value->value.number);
		return PUT_SUCCESS;
	default:
		return DOM_Object::PutName(property_name, value, origining_runtime);
	}
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_CSSMatrix)
	DOM_FUNCTIONS_FUNCTION(DOM_CSSMatrix, DOM_CSSMatrix::setMatrixValue, "setMatrixValue", "s")
	DOM_FUNCTIONS_FUNCTION(DOM_CSSMatrix, DOM_CSSMatrix::multiply, "multiply", "-")
	DOM_FUNCTIONS_FUNCTION(DOM_CSSMatrix, DOM_CSSMatrix::inverse, "inverse", NULL)
	DOM_FUNCTIONS_FUNCTION(DOM_CSSMatrix, DOM_CSSMatrix::translate, "translate", "nn")
	DOM_FUNCTIONS_FUNCTION(DOM_CSSMatrix, DOM_CSSMatrix::scale, "scale", "n")
	DOM_FUNCTIONS_FUNCTION(DOM_CSSMatrix, DOM_CSSMatrix::rotate, "rotate", "n")
DOM_FUNCTIONS_END(DOM_CSSMatrix)

#endif // CSS_TRANSFORMS

