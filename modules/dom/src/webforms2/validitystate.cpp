/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/dom/src/webforms2/validitystate.h"
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/forms/webforms2support.h"
#include "modules/dom/src/domhtml/htmlelem.h"

/* static */
OP_STATUS DOM_ValidityState::Make(DOM_ValidityState*& validity_state,
								  DOM_HTMLElement* html_node,
								  DOM_EnvironmentImpl *environment)
{
	OP_ASSERT(html_node);
	DOM_Runtime *runtime = environment->GetDOMRuntime();
	if (!(validity_state = OP_NEW(DOM_ValidityState, (html_node))) ||
	    OpStatus::IsMemoryError(validity_state->SetObjectRuntime(runtime, runtime->GetObjectPrototype(), "ValidityState")))
	{
		OP_DELETE(validity_state);
		return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}

/* virtual */
ES_GetState	DOM_ValidityState::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
/*
readonly attribute boolean typeMismatch;
readonly attribute boolean stepMismatch;
readonly attribute boolean rangeUnderflow;
readonly attribute boolean rangeOverflow;
readonly attribute boolean tooLong;
readonly attribute boolean patternMismatch;
readonly attribute boolean valueMissing;
readonly attribute boolean customError;
readonly attribute boolean valid; */

	switch(property_name)
	{
	case OP_ATOM_typeMismatch:
	case OP_ATOM_stepMismatch:
	case OP_ATOM_rangeUnderflow:
	case OP_ATOM_rangeOverflow:
	case OP_ATOM_tooLong:
	case OP_ATOM_patternMismatch:
	case OP_ATOM_valueMissing:
	case OP_ATOM_customError:
	case OP_ATOM_valid:
		break;
	default:
		return GET_FAILED;
	}

	FormValidator validator(GetFramesDocument());
	ValidationResult validation_result = validator.ValidateSingleControl(m_html_elm_node->GetThisElement());

	switch(property_name)
	{
	case OP_ATOM_typeMismatch:
		DOMSetBoolean(value, validation_result.HasError(VALIDATE_ERROR_TYPE_MISMATCH));
		break;
	case OP_ATOM_stepMismatch:
		DOMSetBoolean(value, validation_result.HasError(VALIDATE_ERROR_STEP_MISMATCH));
		break;
	case OP_ATOM_rangeUnderflow:
		DOMSetBoolean(value, validation_result.HasError(VALIDATE_ERROR_RANGE_UNDERFLOW));
		break;
	case OP_ATOM_rangeOverflow:
		DOMSetBoolean(value, validation_result.HasError(VALIDATE_ERROR_RANGE_OVERFLOW));
		break;
	case OP_ATOM_tooLong:
		DOMSetBoolean(value, validation_result.HasError(VALIDATE_ERROR_TOO_LONG));
		break;
	case OP_ATOM_patternMismatch:
		DOMSetBoolean(value, validation_result.HasError(VALIDATE_ERROR_PATTERN_MISMATCH));
		break;
	case OP_ATOM_valueMissing:
		DOMSetBoolean(value, validation_result.HasError(VALIDATE_ERROR_REQUIRED));
		break;
	case OP_ATOM_customError:
		DOMSetBoolean(value, validation_result.HasError(VALIDATE_ERROR_CUSTOM));
		break;
	case OP_ATOM_valid:
		DOMSetBoolean(value, validation_result.IsOk());
		break;
#ifdef _DEBUG
	default:
		OP_ASSERT(FALSE); // Not reachable
#endif // _DEBUG
	}
	return GET_SUCCESS;
}

/* virtual */ void
DOM_ValidityState::GCTrace()
{
	GCMark(m_html_elm_node);
}

