/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */
#include "core/pch.h"

#include "modules/inputmanager/inputaction.h"
#include "modules/inputmanager/inputmanager.h"
#include "modules/locale/locale-enum.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/util/OpLineParser.h"
#include "modules/util/simset.h"

#include "modules/hardcore/actions/generated_actions_strings.h"

#include "modules/locale/oplanguagemanager.h"

OP_STATUS OpInputAction::ConvertToString(OpString& string)
{
	OpLineString line;

	for (OpInputAction* input_action = this; input_action; input_action = input_action->GetNextInputAction())
	{
		if (OpStatus::IsError(line.WriteToken8(GetStringFromAction(input_action->GetAction())))) goto failure;

		if (input_action->HasActionDataString())
		{
			if (OpStatus::IsError(line.WriteString(input_action->GetActionDataString()))) goto failure;

			if (input_action->HasActionDataStringParameter())
			{
				if (OpStatus::IsError(line.WriteString(input_action->GetActionDataStringParameter()))) goto failure;
			}
			else
			{
				if (OpStatus::IsError(line.WriteValue(input_action->GetActionData()))) goto failure;
			}
		}
		else
		{
			if (OpStatus::IsError(line.WriteValue(input_action->GetActionData()))) goto failure;
			if (OpStatus::IsError(line.WriteEmpty())) goto failure;
		}

		if (input_action->HasActionTextID())
		{
			if (OpStatus::IsError(line.WriteValue(input_action->GetActionTextID()))) goto failure;
		}
		else
		{
			if (OpStatus::IsError(line.WriteString(input_action->GetActionText()))) goto failure;
		}

		if (OpStatus::IsError(line.WriteString8(input_action->GetActionImage(FALSE)))) goto failure;

		if (input_action->GetNextInputAction())
		{
			switch (input_action->GetActionOperator())
			{
			case OPERATOR_OR:   if (OpStatus::IsError(line.WriteSeparator('|'))) goto failure; break;
			case OPERATOR_AND:  if (OpStatus::IsError(line.WriteSeparator('&'))) goto failure; break;
			case OPERATOR_NEXT: if (OpStatus::IsError(line.WriteSeparator('>'))) goto failure; break;
			case OPERATOR_PLUS: if (OpStatus::IsError(line.WriteSeparator('+'))) goto failure; break;
			}
		}
	}

	if (OpStatus::IsError(string.Set(line.GetString()))) goto failure;

	return OpStatus::OK;

failure:
	string.Empty();
	return OpStatus::ERR_NO_MEMORY;
}

OpInputAction* OpInputAction::CopyInputActions(OpInputAction* source_action, INT32 stop_at_operator)
{
	OpInputAction* current_input_action = NULL;
	OpInputAction* first_input_action = NULL;

	while (source_action)
	{
		OpInputAction* new_input_action = OP_NEW(OpInputAction, ());

		if (new_input_action == NULL)
		{
			OP_DELETE(first_input_action);
			return NULL;
		}
		OP_STATUS status = new_input_action->Clone(source_action);
		if (OpStatus::IsError(status))
		{
			if (OpStatus::IsMemoryError(status))
			{
				g_memory_manager->RaiseCondition(status);
			}
			OP_DELETE(first_input_action);
			OP_DELETE(new_input_action);
			return NULL;
		}

		if (current_input_action)
		{
			current_input_action->m_next_input_action = new_input_action;
		}
		else
		{
			first_input_action = new_input_action;
		}

		if (stop_at_operator != OPERATOR_NONE && (source_action->GetActionOperator() & stop_at_operator))
		{
			new_input_action->SetActionOperator(OPERATOR_NONE);
			break;
		}

		current_input_action = new_input_action;
		source_action = source_action->GetNextInputAction();
	}

	return first_input_action;
}

OpInputAction* OpInputAction::CreateToggleAction(Action action1, Action action2, INTPTR action_data, const uni_char* action_data_string)
{
	OpInputAction
		*input_action1 = NULL,
		*input_action2 = NULL;

	input_action1 = OP_NEW(OpInputAction, (action1));
	if (input_action1)
	{
		input_action1->SetActionData(action_data);
		input_action1->SetActionDataString(action_data_string);
		input_action1->SetActionOperator(OpInputAction::OPERATOR_OR);

		input_action2 = OP_NEW(OpInputAction, (action2));
		if (input_action2)
		{
			input_action2->SetActionData(action_data);
			input_action2->SetActionDataString(action_data_string);

			input_action1->SetNextInputAction(input_action2);
			return input_action1;
		}

		OP_DELETE(input_action1);
	}

	return NULL;
}

OP_STATUS OpInputAction::CreateInputActionsFromString(const uni_char* string, OpInputAction*& input_action, INTPTR default_value, BOOL /*keyboard_invoked*/)
{
	OpString buffer;
	RETURN_IF_ERROR(buffer.Set(string));

	uni_char* current = buffer.CStr();

	OpAutoPtr<OpInputAction> action_anchor;
	OpInputAction* current_input_action = NULL;

	BOOL inside_string = FALSE;

	while (current && *current)
	{
		ActionOperator action_operator = current_input_action ? current_input_action->GetActionOperator() : OPERATOR_NONE;

		uni_char* next = current;

		BOOL found_operator = FALSE;

		while (*next && !found_operator)
		{
			if( *next == '"')
			{
				inside_string = !inside_string;
			}
			else if( !inside_string )
			{
				switch (*next)
				{
					case '|': action_operator = OPERATOR_OR; *next = 0; found_operator = TRUE; break;
					case '&': action_operator = OPERATOR_AND; *next = 0; found_operator = TRUE; break;
					case '>': action_operator = OPERATOR_NEXT; *next = 0; found_operator = TRUE; break;
					case '+': action_operator = OPERATOR_PLUS; *next = 0; found_operator = TRUE; break;
				}
			}

			next++;
		}

		OpLineParser line(current);

		OpString8 action_name;
		OpString action_data_string;
		OpString action_data_string_parameter;
		OpString action_text;
		Str::LocaleString action_text_id(Str::NOT_A_STRING);
		OpString action_image;
		INTPTR action_data = 0;

		RETURN_IF_MEMORY_ERROR(line.GetNextToken8(action_name));

		INT32 tempaction_data = INT_MAX;
		RETURN_IF_MEMORY_ERROR(line.GetNextStringOrValue(action_data_string, tempaction_data));
		if (tempaction_data == INT_MAX)
		{
			RETURN_IF_MEMORY_ERROR(line.GetNextStringOrValue(action_data_string_parameter, tempaction_data));
			if (tempaction_data == INT_MAX)
				action_data = default_value;
			else
				action_data = tempaction_data;
		}
		else
		{
			RETURN_IF_MEMORY_ERROR(line.GetNextString(action_data_string));
			action_data = tempaction_data; // tempaction_data is 32 bit, action data can be 64 bit.
		}

		if (line.HasContent())
			RETURN_IF_MEMORY_ERROR(line.GetNextLanguageString(action_text, &action_text_id));
		RETURN_IF_MEMORY_ERROR(line.GetNextString(action_image));

		if (action_data == -2)
			action_data = default_value;

		Action action;
		RETURN_IF_ERROR(g_input_manager->GetActionFromString(action_name.CStr(), &action));

#ifdef QUICK
		// convert various obsolete actions into new ones to be backward compatible

		switch (action)
		{
			case ACTION_VIEW_HOTLIST:
				action_data_string.Set(UNI_L("hotlist"));
				break;

			case ACTION_VIEW_MAIN_BAR:
				action_data_string.Set(UNI_L("browser toolbar"));
				break;

			case ACTION_VIEW_STATUS_BAR:
				action_data_string.Set(UNI_L("status toolbar"));
				break;

			case ACTION_VIEW_PERSONAL_BAR:
				action_data_string.Set(UNI_L("personalbar"));
				break;

			case ACTION_VIEW_PAGE_BAR:
				action_data_string.Set(UNI_L("pagebar"));
				break;

			case ACTION_VIEW_ADDRESS_BAR:
				action_data_string.Set(UNI_L("document toolbar"));
				break;

			case ACTION_VIEW_NAVIGATION_BAR:
				action_data_string.Set(UNI_L("site navigation toolbar"));
				break;
		}

		switch (action)
		{
			case ACTION_VIEW_HOTLIST:
			case ACTION_VIEW_MAIN_BAR:
			case ACTION_VIEW_STATUS_BAR:
			case ACTION_VIEW_PERSONAL_BAR:
			case ACTION_VIEW_PAGE_BAR:
			case ACTION_VIEW_ADDRESS_BAR:
			case ACTION_VIEW_NAVIGATION_BAR:
			action = ACTION_SET_ALIGNMENT;
			if (action_data == 7) action_data = 6;
				break;
		}
#endif

		OpInputAction* new_input_action;

		if (action != ACTION_UNKNOWN)
		{
			new_input_action = OP_NEW(OpInputAction, (action));
			if (new_input_action)
			{
				new_input_action->SetActionData(action_data);
				new_input_action->SetActionDataString(action_data_string.CStr());
				new_input_action->SetActionDataStringParameter(action_data_string_parameter.CStr());
				new_input_action->SetActionText(action_text.CStr());
				new_input_action->SetActionTextID(action_text_id);

				OpString8 action_image8;
				action_image8.Set(action_image.CStr());
				new_input_action->SetActionImage(action_image8.CStr());

				new_input_action->SetActionOperator(action_operator);
			}

  		}
		else
		{
			new_input_action = OP_NEW(OpInputAction, (ACTION_EXTERNAL_ACTION));
			if (new_input_action)
			{
				new_input_action->SetActionDataString(current);
				new_input_action->SetActionTextID(action_text_id);
				new_input_action->SetActionOperator(action_operator);
			}
		}

		if (new_input_action == NULL)
		{
			return OpStatus::ERR_NO_MEMORY;
		}

		if (current_input_action)
		{
			current_input_action->m_next_input_action = new_input_action;
		}
		else
		{
			OP_ASSERT(!action_anchor.get()); // The anchor should be empty the first time
			action_anchor.reset(new_input_action);
		}

		current_input_action = new_input_action;

		current = next;
	}

	input_action = action_anchor.release();
	return input_action ? OpStatus::OK : OpStatus::ERR;
}

OpInputAction::~OpInputAction()
{
	OP_DELETE(m_next_input_action);
    if (m_key_event_data)
		OpPlatformKeyEventData::DecRef(m_key_event_data);
}

void OpInputAction::DeleteInputActions(OpInputAction* input_action)
{
	OP_DELETE(input_action);
}

/*static*/
OP_STATUS OpInputAction::GetStringFromAction(OpInputAction::Action action, OpString8& string)
{
	return string.Set(s_action_strings[action]);
}

/*static*/
const char* OpInputAction::GetStringFromAction(OpInputAction::Action action)
{
	return s_action_strings[action];
}

/*static*/
OP_STATUS OpInputAction::GetLocaleStringFromAction(OpInputAction::Action action, OpString& string)
{
	switch (action)
	{
#ifdef       ACTION_BACK_ENABLED
		case ACTION_BACK:						  return g_languageManager->GetString(Str::SI_PREV_BUTTON_TEXT, string);
#endif
#ifdef       ACTION_REWIND_ENABLED
		case ACTION_REWIND:						  return g_languageManager->GetString(Str::M_REWIND, string);
#endif
#ifdef       ACTION_FORWARD_ENABLED
		case ACTION_FORWARD:					  return g_languageManager->GetString(Str::SI_NEXT_BUTTON_TEXT, string);
#endif
#ifdef       ACTION_FAST_FORWARD_ENABLED
		case ACTION_FAST_FORWARD:				  return g_languageManager->GetString(Str::S_FAST_FORWARD, string);
#endif
#ifdef       ACTION_STOP_ENABLED
		case ACTION_STOP:						  return g_languageManager->GetString(Str::SI_STOP_BUTTON_TEXT, string);
#endif
#ifdef       ACTION_RELOAD_ENABLED
		case ACTION_RELOAD:						  return g_languageManager->GetString(Str::SI_RELOAD_BUTTON_TEXT, string);
#endif
#ifdef       ACTION_FORCE_RELOAD_ENABLED
		// Using same string as Reload because this is more a tweak on the action than a different action.
		case ACTION_FORCE_RELOAD:						  return g_languageManager->GetString(Str::SI_RELOAD_BUTTON_TEXT, string);
#endif
#ifdef       ACTION_GO_TO_PARENT_DIRECTORY_ENABLED
		case ACTION_GO_TO_PARENT_DIRECTORY:		  return g_languageManager->GetString(Str::S_GO_TO_PARENT_DIRECTORY, string);
#endif
#ifdef       ACTION_MAXIMIZE_PAGE_ENABLED
		case ACTION_MAXIMIZE_PAGE:				  return g_languageManager->GetString(Str::M_PAGE_MAXIMIZE, string);
#endif
#ifdef       ACTION_OPEN_LINK_IN_NEW_PAGE_ENABLED
		case ACTION_OPEN_LINK_IN_NEW_PAGE:		  return g_languageManager->GetString(Str::D_MIDCLICKDIALOG_OPEN_IN_NEW_PAGE, string);
#endif
#ifdef       ACTION_MINIMIZE_PAGE_ENABLED
		case ACTION_MINIMIZE_PAGE:				  return g_languageManager->GetString(Str::M_MINIMIZE_PAGE, string);
#endif
#ifdef       ACTION_CLOSE_PAGE_ENABLED
		case ACTION_CLOSE_PAGE:					  return g_languageManager->GetString(Str::S_CLOSE_TAB_MOUSE_GESTURE, string);
#endif
#ifdef       ACTION_OPEN_LINK_IN_BACKGROUND_PAGE_ENABLED
		case ACTION_OPEN_LINK_IN_BACKGROUND_PAGE: return g_languageManager->GetString(Str::D_MIDCLICKDIALOG_OPEN_IN_BACKGROUNDPAGE, string);
#endif
#ifdef       ACTION_RESTORE_PAGE_ENABLED
		case ACTION_RESTORE_PAGE:				  return g_languageManager->GetString(Str::M_RESTORE_PAGE, string);
#endif
#ifdef       ACTION_NEW_PAGE_ENABLED
		case ACTION_NEW_PAGE:					  return g_languageManager->GetString(Str::S_NEW_PAGE, string);
#endif
#ifdef       ACTION_DUPLICATE_PAGE_ENABLED
		case ACTION_DUPLICATE_PAGE:				  return g_languageManager->GetString(Str::S_DUPLICATE_PAGE, string);
#endif
#ifdef       ACTION_OPEN_ALL_ITEMS_ENABLED
		case ACTION_OPEN_ALL_ITEMS:				  return g_languageManager->GetString(Str::S_OPEN_ALL_ITEMS, string);
#endif
#ifdef       ACTION_CLOSE_ALL_ITEMS_ENABLED
		case ACTION_CLOSE_ALL_ITEMS:			  return g_languageManager->GetString(Str::S_CLOSE_ALL_ITEMS, string);
#endif

		default:								  return string.Set(s_action_strings[action]);
	}
}


BOOL OpInputAction::Equals(OpInputAction* action)
{
	if (!action)
		return FALSE;

	BOOL equal = (m_action == action->m_action && m_action_data == action->m_action_data &&	m_action_data_string.CompareI(action->m_action_data_string) == 0 && (!m_next_input_action || m_action_operator == action->m_action_operator));

	if (!equal)
		return FALSE;

	if (m_next_input_action)
		return m_next_input_action->Equals(action->m_next_input_action);

	return !action->m_next_input_action;
}

void OpInputAction::SetEnabled(BOOL enabled)
{
	if (enabled)
	{
		m_action_state |= STATE_ENABLED;
		m_action_state &= ~STATE_DISABLED;
	}
	else
	{
		m_action_state &= ~STATE_ENABLED;
		m_action_state |= STATE_DISABLED;
	}
}

void OpInputAction::SetSelected(BOOL selected)
{
	if (selected)
	{
		m_action_state |= STATE_SELECTED;
		m_action_state &= ~STATE_UNSELECTED;
	}
	else
	{
		m_action_state &= ~STATE_SELECTED;
		m_action_state |= STATE_UNSELECTED;
	}
}

void OpInputAction::SetSelectedByToggleAction(Action true_action, BOOL toggle)
{
	if (m_action == true_action)
		SetSelected(toggle);
	else
		SetSelected(!toggle);
}

void OpInputAction::SetAttention(BOOL attention)
{
	if (attention)
	{
		m_action_state |= STATE_ATTENTION;
		m_action_state &= ~STATE_INATTENTION;
	}
	else
	{
		m_action_state &= ~STATE_ATTENTION;
		m_action_state |= STATE_INATTENTION;
	}
}

void
OpInputAction::SetActionDataString(const uni_char* string)
{
	//rg this is not optimal -- will fix properly
	OP_STATUS status = m_action_data_string.Set(string);
	if (OpStatus::IsMemoryError(status))
	{
		g_memory_manager->RaiseCondition(status);
	}
}

void
OpInputAction::SetActionDataStringParameter(const uni_char* string)
{
	OP_STATUS status = m_action_data_string_parameter.Set(string);
	if (OpStatus::IsMemoryError(status))
	{
		g_memory_manager->RaiseCondition(status);
	}
}

OP_STATUS OpInputAction::SetActionImage(const char* string)
{
	return m_action_image.Set(string);
}

void
OpInputAction::SetActionText(const uni_char* string)
{
	//rg this is not optimal -- will fix properly
	OP_STATUS status = m_action_text.Set(string);
	if (OpStatus::IsMemoryError(status))
	{
		g_memory_manager->RaiseCondition(status);
	}
}

void
OpInputAction::SetActionTextID(Str::LocaleString string_id)
{
	m_action_text_id = string_id;
}

void
OpInputAction::SetActionPosition(const OpRect& rect)
{
	m_action_position = rect;
	if (m_next_input_action)
		m_next_input_action->SetActionPosition(rect);
}

void
OpInputAction::SetActionPosition(const OpPoint& point)
{
	SetActionPosition(OpRect(point.x, point.y, 1, 1));
}

void
OpInputAction::SetPlatformKeyEventData(OpPlatformKeyEventData *data)
{
	OP_ASSERT(m_key_event_data == NULL);
	if (data)
		OpPlatformKeyEventData::IncRef(data);
	m_key_event_data = data;
}

OP_STATUS
OpInputAction::SetActionKeyValue(const char *value, unsigned length)
{
	if (length > 0)
		return m_key_value.SetFromUTF8(value, length);
	else
	{
		m_key_value.Empty();
		return OpStatus::OK;
	}
}

OP_STATUS
OpInputAction::SetActionKeyValue(const uni_char *value)
{
	if (value)
		return m_key_value.Set(value);
	else
	{
		m_key_value.Empty();
		return OpStatus::OK;
	}
}

OpInputAction::OpInputAction()
{
	SetMembers(ACTION_UNKNOWN);
}

OpInputAction::OpInputAction(Action action)
{
	SetMembers(action);
}

OpInputAction::OpInputAction(Action action, OpKey::Code key, ShiftKeyState shift_keys, BOOL repeat, OpKey::Location location, ActionMethod action_method)
{
	SetMembers(action);
	m_action_data = key;
	m_shift_keys = shift_keys;
	m_key_repeat = repeat;
	m_key_location = location;
	m_action_method = action_method;
}

// ACTION_LOWLEVEL_PREFILTER_ACTION

OpInputAction::OpInputAction(OpInputAction* child_action, Action action)
{
	SetMembers(action);
	m_child_action = child_action;
}

OP_STATUS
OpInputAction::Clone(OpInputAction* src_action)
{
	m_next_input_action = NULL;
	m_action_position = src_action->m_action_position;
	m_child_action = src_action->m_child_action;
	m_action_operator = src_action->m_action_operator;
	m_action = src_action->m_action;
	m_action_data = src_action->m_action_data;
	m_action_object = src_action->m_action_object;
	m_key_repeat = src_action->m_key_repeat;
	m_key_location = src_action->m_key_location;
	m_shift_keys = src_action->m_shift_keys;
	m_repeat_count = src_action->m_repeat_count;
	m_action_state = src_action->m_action_state;
	m_action_method = src_action->m_action_method;
	m_first_input_context = src_action->m_first_input_context;
	m_was_really_handled = src_action->m_was_really_handled;
	m_gesture_invoked = src_action->m_gesture_invoked;
	m_referrer_action = src_action->m_referrer_action;
	m_action_text_id = src_action->m_action_text_id;

	RETURN_IF_ERROR(m_action_data_string.Set(src_action->m_action_data_string));
	RETURN_IF_ERROR(m_action_data_string_parameter.Set(src_action->m_action_data_string_parameter));
	RETURN_IF_ERROR(m_action_data_string.Set(src_action->m_action_data_string));
	RETURN_IF_ERROR(m_action_data_string_parameter.Set(src_action->m_action_data_string_parameter));
	RETURN_IF_ERROR(m_action_text.Set(src_action->m_action_text));
	RETURN_IF_ERROR(m_action_image.Set(src_action->m_action_image));

	RETURN_IF_ERROR(m_action_info.Copy(src_action->m_action_info));
	RETURN_IF_ERROR(m_key_value.Set(src_action->m_key_value));

	return OpStatus::OK;
}

Str::LocaleString OpInputAction::GetActionTextID()
{
	return Str::LocaleString(m_action_text_id);
}

OpInputAction* OpInputAction::GetNextInputAction(ActionOperator after_this_operator)
{
	if (after_this_operator == OPERATOR_NONE)
	{
		return m_next_input_action;
	}
	else
	{
		OpInputAction* action = this;

		while (action)
		{
			if (action->GetActionOperator() == after_this_operator)
			{
				return action->GetNextInputAction();
			}
			action = action->GetNextInputAction();
		}
	}

	return NULL;
}

void OpInputAction::SetMembers(Action action)
{
	m_action_text_id = Str::NOT_A_STRING;
	m_action = action;
	m_next_input_action = NULL;
	m_action_operator = OpInputAction::OPERATOR_NONE;
	m_action_data = 0;
	m_shift_keys = SHIFTKEY_NONE;
	m_key_repeat = FALSE;
	m_key_location = OpKey::LOCATION_STANDARD;
	m_repeat_count = 0;
	m_key_event_data = NULL;
	m_child_action = NULL;
	m_action_state = STATE_ENABLED;
	m_action_object = NULL;
	m_action_method = METHOD_OTHER;
	m_first_input_context = NULL;
	m_was_really_handled = FALSE;
	m_gesture_invoked = FALSE;
	m_referrer_action = ACTION_UNKNOWN;
}

BOOL OpInputAction::IsMoveAction()
{
	switch(m_action)
	{
#ifdef ACTION_GO_TO_START_ENABLED
	case ACTION_GO_TO_START:
#endif //ACTION_GO_TO_START_ENABLED

#ifdef ACTION_GO_TO_END_ENABLED
	case ACTION_GO_TO_END:
#endif //ACTION_GO_TO_END_ENABLED

#ifdef ACTION_GO_TO_CONTENT_MAGIC_ENABLED
	case ACTION_GO_TO_CONTENT_MAGIC:
#endif //ACTION_GO_TO_CONTENT_MAGIC_ENABLED

#ifdef ACTION_GO_TO_TOP_CM_BOTTOM_ENABLED
	case ACTION_GO_TO_TOP_CM_BOTTOM:
#endif //ACTION_GO_TO_TOP_CM_BOTTOM_ENABLED

#ifdef ACTION_GO_TO_LINE_START_ENABLED
	case ACTION_GO_TO_LINE_START:
#endif //ACTION_GO_TO_LINE_START_ENABLED

#ifdef ACTION_GO_TO_LINE_END_ENABLED
	case ACTION_GO_TO_LINE_END:
#endif //ACTION_GO_TO_LINE_END_ENABLED

#ifdef ACTION_PAGE_UP_ENABLED
	case ACTION_PAGE_UP:
#endif //ACTION_PAGE_UP_ENABLED

#ifdef ACTION_PAGE_DOWN_ENABLED
	case ACTION_PAGE_DOWN:
#endif //ACTION_PAGE_DOWN_ENABLED

#ifdef ACTION_PAGE_LEFT_ENABLED
	case ACTION_PAGE_LEFT:
#endif //ACTION_PAGE_LEFT_ENABLED

#ifdef ACTION_PAGE_RIGHT_ENABLED
	case ACTION_PAGE_RIGHT:
#endif //ACTION_PAGE_RIGHT_ENABLED

#ifdef ACTION_NEXT_ITEM_ENABLED
	case ACTION_NEXT_ITEM:
#endif //ACTION_NEXT_ITEM_ENABLED

#ifdef ACTION_PREVIOUS_ITEM_ENABLED
	case ACTION_PREVIOUS_ITEM:
#endif //ACTION_PREVIOUS_ITEM_ENABLED

#ifdef ACTION_NEXT_CHARACTER_ENABLED
	case ACTION_NEXT_CHARACTER:
#endif //ACTION_NEXT_CHARACTER_ENABLED

#ifdef ACTION_PREVIOUS_CHARACTER_ENABLED
	case ACTION_PREVIOUS_CHARACTER:
#endif //ACTION_PREVIOUS_CHARACTER_ENABLED

#ifdef ACTION_NEXT_WORD_ENABLED
	case ACTION_NEXT_WORD:
#endif //ACTION_NEXT_WORD_ENABLED

#ifdef ACTION_PREVIOUS_WORD_ENABLED
	case ACTION_PREVIOUS_WORD:
#endif //ACTION_PREVIOUS_WORD_ENABLED

#ifdef ACTION_NEXT_LINE_ENABLED
	case ACTION_NEXT_LINE:
#endif //ACTION_NEXT_LINE_ENABLED

#ifdef ACTION_PREVIOUS_LINE_ENABLED
	case ACTION_PREVIOUS_LINE:
#endif //ACTION_PREVIOUS_LINE_ENABLED
		return TRUE;

	default:
		return FALSE;
	}
}

BOOL OpInputAction::IsRangeAction()
{
	switch(m_action)
	{
#ifdef ACTION_RANGE_GO_TO_START_ENABLED
	case ACTION_RANGE_GO_TO_START:
#endif //ACTION_RANGE_GO_TO_START_ENABLED

#ifdef ACTION_RANGE_GO_TO_END_ENABLED
	case ACTION_RANGE_GO_TO_END:
#endif //ACTION_RANGE_GO_TO_END_ENABLED

#ifdef ACTION_RANGE_GO_TO_LINE_START_ENABLED
	case ACTION_RANGE_GO_TO_LINE_START:
#endif //ACTION_RANGE_GO_TO_LINE_START_ENABLED

#ifdef ACTION_RANGE_GO_TO_LINE_END_ENABLED
	case ACTION_RANGE_GO_TO_LINE_END:
#endif //ACTION_RANGE_GO_TO_LINE_END_ENABLED

#ifdef ACTION_RANGE_PAGE_UP_ENABLED
	case ACTION_RANGE_PAGE_UP:
#endif //ACTION_RANGE_PAGE_UP_ENABLED

#ifdef ACTION_RANGE_PAGE_DOWN_ENABLED
	case ACTION_RANGE_PAGE_DOWN:
#endif //ACTION_RANGE_PAGE_DOWN_ENABLED

#ifdef ACTION_RANGE_PAGE_LEFT_ENABLED
	case ACTION_RANGE_PAGE_LEFT:
#endif //ACTION_RANGE_PAGE_LEFT_ENABLED

#ifdef ACTION_RANGE_PAGE_RIGHT_ENABLED
	case ACTION_RANGE_PAGE_RIGHT:
#endif //ACTION_RANGE_PAGE_RIGHT_ENABLED

#ifdef ACTION_RANGE_NEXT_ITEM_ENABLED
	case ACTION_RANGE_NEXT_ITEM:
#endif //ACTION_RANGE_NEXT_ITEM_ENABLED

#ifdef ACTION_RANGE_PREVIOUS_ITEM_ENABLED
	case ACTION_RANGE_PREVIOUS_ITEM:
#endif //ACTION_RANGE_PREVIOUS_ITEM_ENABLED

#ifdef ACTION_RANGE_NEXT_CHARACTER_ENABLED
	case ACTION_RANGE_NEXT_CHARACTER:
#endif //ACTION_RANGE_NEXT_CHARACTER_ENABLED

#ifdef ACTION_RANGE_PREVIOUS_CHARACTER_ENABLED
	case ACTION_RANGE_PREVIOUS_CHARACTER:
#endif //ACTION_RANGE_PREVIOUS_CHARACTER_ENABLED

#ifdef ACTION_RANGE_NEXT_WORD_ENABLED
	case ACTION_RANGE_NEXT_WORD:
#endif //ACTION_RANGE_NEXT_WORD_ENABLED

#ifdef ACTION_RANGE_PREVIOUS_WORD_ENABLED
	case ACTION_RANGE_PREVIOUS_WORD:
#endif //ACTION_RANGE_PREVIOUS_WORD_ENABLED

#ifdef ACTION_RANGE_NEXT_LINE_ENABLED
	case ACTION_RANGE_NEXT_LINE:
#endif //ACTION_RANGE_NEXT_LINE_ENABLED

#ifdef ACTION_RANGE_PREVIOUS_LINE_ENABLED
	case ACTION_RANGE_PREVIOUS_LINE:
#endif //ACTION_RANGE_PREVIOUS_LINE_ENABLED
		return TRUE;
	default:
		return FALSE;
	}
}

BOOL OpInputAction::IsLowlevelAction()
{
	switch(m_action)
	{
	case ACTION_LOWLEVEL_KEY_DOWN:
	case ACTION_LOWLEVEL_KEY_UP:
	case ACTION_LOWLEVEL_KEY_PRESSED:
	case ACTION_LOWLEVEL_NEW_KEYBOARD_CONTEXT:
	case ACTION_LOWLEVEL_NEW_MOUSE_CONTEXT:
	case ACTION_LOWLEVEL_PREFILTER_ACTION:
		return TRUE;
	default:
		return FALSE;
	}
}

OP_STATUS OpInfoText::SetStatusText(const uni_char* text)
{
	if (text)
	{
		INT32 full_len = uni_strlen(text);
		INT32 len = full_len;

		if (m_auto_truncate)
		{
			const uni_char* newline = uni_strchr(text, '\r');

			if (!newline)
			{
				newline = uni_strchr(text, '\n');
			}

			if (newline)
			{
				len = newline - text;
			}

			if (len > max_line_width)
			{
				len = max_line_width;
			}
		}

		RETURN_IF_ERROR(m_status_text.Set(text, len));

		if (full_len > len)
		{
			RETURN_IF_ERROR(m_status_text.Append(UNI_L("...")));
		}
	}
	else
	{
		m_status_text.Empty();
	}

	return OpStatus::OK;
}

OP_STATUS OpInfoText::AddTooltipText(const uni_char* label, const uni_char* text)
{
	if (!text || !*text)
		return OpStatus::OK;

	INT32 label_len = label ? uni_strlen(label) : 0;
	INT32 text_len  = uni_strlen(text);

	if (m_tooltip_text.HasContent())
	{
		RETURN_IF_ERROR(m_tooltip_text.Append(UNI_L("\n")));
	}

	if (label_len)
	{
		RETURN_IF_ERROR(m_tooltip_text.Append(label));
		RETURN_IF_ERROR(m_tooltip_text.Append(UNI_L(": ")));
	}

	int num_lines = 0;

	OpString newline_string;
	RETURN_IF_ERROR(newline_string.Set(NEWLINE));
	INT32 newline_len = newline_string.Length();

	uni_char *last_newline_pos = NULL;

	do
	{
		if (newline_len && !last_newline_pos)
		{
			last_newline_pos = uni_strstr(text, newline_string.CStr());
			if (!last_newline_pos)
				newline_len = 0;				// don't search again
		}

		INT32 len = last_newline_pos ? last_newline_pos - text : text_len;

		if (len > max_line_width)
		{
			const uni_char* break_after_chars = UNI_L("&,./\\ ");
			const uni_char* break_after = text + max_line_width;
			const uni_char* found_break;

			do
			{
				break_after--;
				found_break = uni_strchr(break_after_chars, *break_after);
			}
			while (!found_break && break_after > text);

			if (found_break)
			{
				len = break_after - text + 1;
			}
			else
			{
				len = max_line_width;
			}
		}

		if (len)								// strip empty lines
		{
			if (num_lines)
			{
				RETURN_IF_ERROR(m_tooltip_text.Append(UNI_L("\n")));
				if (label_len)
				{
					RETURN_IF_ERROR(m_tooltip_text.Append(UNI_L("\t\t")));
				}
			}
			RETURN_IF_ERROR(m_tooltip_text.Append(text, len));
			num_lines++;
			text += len;
			text_len -= len;
		}

		if (text == last_newline_pos)
		{
			text += newline_len;
			text_len -= newline_len;
			last_newline_pos = NULL;
		}
	}
	while (text_len && num_lines < max_lines);

	return OpStatus::OK;
}

OP_STATUS OpInfoText::Copy(OpInfoText& src)
{
	RETURN_IF_ERROR(m_status_text.Set(src.m_status_text));
	RETURN_IF_ERROR(m_tooltip_text.Set(src.m_tooltip_text));

	return OpStatus::OK;
}

void OpInputState::SetEnabled(BOOL enabled)
{
	if (enabled)
		g_input_manager->AddInputState(this);
	else
		g_input_manager->RemoveInputState(this);
}
