/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/creators/QuickActionCreator.h"
#include "modules/inputmanager/inputmanager.h"
#include "modules/locale/locale-enum.h"

OP_STATUS
QuickActionCreator::CreateInputAction(OpAutoPtr<OpInputAction>& action)
{
	OpString8 action_name;
	RETURN_IF_ERROR(GetScalarStringFromMap("name", action_name));
	if (action_name.IsEmpty())
	{
		return OpStatus::ERR;
	}
	OpInputAction::Action act;
	RETURN_IF_ERROR(g_input_manager->GetActionFromString(action_name.CStr(), &act));

	INT32 action_data = 0;
	RETURN_IF_ERROR(GetScalarIntFromMap("data_number", action_data));

	OpString8 action_data_string8;
	RETURN_IF_ERROR(GetScalarStringFromMap("data_string", action_data_string8));
	OpString action_data_string;
	RETURN_IF_ERROR(action_data_string.Set(action_data_string8));

	// do we need support for data_string_parameter? leaving out for now.
	// not supporting action text and icon on purpose (should be specified outside of the actino)

	OpAutoPtr<OpInputAction> next_action;
	RETURN_IF_ERROR(CreateInputActionFromMap(next_action));

	OpString8 action_operator_str;
	RETURN_IF_ERROR(GetScalarStringFromMap("operator", action_operator_str));

	OpInputAction::ActionOperator action_operator = OpInputAction::OPERATOR_NONE;
	if (action_operator_str.HasContent())
	{
		if (action_operator_str.CompareI("and") == 0 || action_operator_str.Compare("&") == 0)
		{
			action_operator = OpInputAction::OPERATOR_AND;
		}
		else if (action_operator_str.CompareI("or") == 0 || action_operator_str.Compare("|") == 0)
		{
			action_operator = OpInputAction::OPERATOR_OR;
		}
		else if (action_operator_str.CompareI("next") == 0 || action_operator_str.Compare(">") == 0)
		{
			action_operator = OpInputAction::OPERATOR_NEXT;
		}
		else // no support for PLUS operator, since it seems to be a purely visual operator, not a logical one
		{
			OP_ASSERT(!"unknown/unsupported action operator");
		}

	}
	action.reset(OP_NEW(OpInputAction, (act)));
	if (!action.get())
		return OpStatus::ERR_NO_MEMORY;

	action->SetActionData(static_cast<INTPTR>(action_data));
	action->SetActionDataString(action_data_string.CStr());
	action->SetActionTextID(Str::NOT_A_STRING);
	action->SetActionOperator(action_operator);

	action->SetNextInputAction(next_action.release());

	return OpStatus::OK;
}
