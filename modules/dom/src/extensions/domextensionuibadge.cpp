/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA. All rights reserved.
**
** This file is part of the Opera web browser. It may not be distributed
** under any circumstances.
*/

#include <core/pch.h>

#ifdef EXTENSION_SUPPORT
#include "modules/dom/src/extensions/domextensionuibadge.h"
#include "modules/dom/src/extensions/domextensionuiitem.h"
#include "modules/dom/src/canvas/domcontext2d.h"
#include "modules/stdlib/include/double_format.h"

/* static */ OP_STATUS
DOM_ExtensionUIBadge::Make(DOM_Object *this_object, DOM_ExtensionUIBadge *&new_obj, ES_Object *properties, DOM_ExtensionUIItem *owner, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	RETURN_IF_ERROR(DOMSetObjectRuntime(new_obj = OP_NEW(DOM_ExtensionUIBadge, ()), origining_runtime, origining_runtime->GetPrototype(DOM_Runtime::EXTENSION_UIBADGE_PROTOTYPE), "Badge"));

	return new_obj->Initialize(this_object, properties, owner, return_value, origining_runtime);
}

static OP_STATUS
ConstructUIColor(DOM_Object *this_object, UINT32 &color_val, ES_Value &value, ES_Value *return_value, ES_Runtime *origining_runtime)
{
	if (value.type == VALUE_STRING)
	{
		const uni_char *col_string = value.value.string;
		COLORREF col;
		if (!ParseColor(col_string, uni_strlen(col_string), col))
		{
			this_object->CallDOMException(DOM_Object::NOT_SUPPORTED_ERR, return_value);
			return OpStatus::ERR;
		}
		else
		{
			color_val = col;
			return OpStatus::OK;
		}
	}
	else if (value.type == VALUE_OBJECT)
	{
		ES_Object *color_array = value.value.object;
		unsigned char rgba[4]; /* ARRAY OK 2010-09-08 sof */
		ES_Value color_value;

		for (unsigned i = 0; i < 4; i++)
		{
			OP_BOOLEAN result;
			if (OpBoolean::IsError(result = origining_runtime->GetIndex(color_array, i, &color_value)))
			{
				if (i == 3)
				{
					rgba[3] = 0xff;
					break;
				}
				else
					return result;
			}
			else if (result == OpBoolean::IS_FALSE || color_value.type != VALUE_NUMBER || !op_isfinite(value.value.number))
			{
				this_object->CallDOMException(DOM_Object::NOT_SUPPORTED_ERR, return_value);
				return OpStatus::ERR;
			}
			else
			{
				double d = color_value.value.number;
				if (d < 0 || d > 255)
				{
					this_object->CallDOMException(DOM_Object::NOT_SUPPORTED_ERR, return_value);
					return OpStatus::ERR;
				}
				else
					rgba[i] = static_cast<unsigned char>(op_truncate(d));
			}
		}
		color_val = OP_RGBA(rgba[0], rgba[1], rgba[2], rgba[3]);
		return OpStatus::OK;
	}
	else
	{
		this_object->CallDOMException(DOM_Object::NOT_SUPPORTED_ERR, return_value);
		return OpStatus::ERR;
	}
}

/* static */ int
DOM_ExtensionUIBadge::update(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(badge, DOM_TYPE_EXTENSION_UIBADGE, DOM_ExtensionUIBadge);

	if (argc < 0)
	{
		badge->m_owner->SetBlockedThread(NULL);
		badge->m_owner->GetThreadListener()->Remove();
		OpExtensionUIListener::ItemAddedStatus call_status = badge->m_owner->GetAddedCallStatus();
		badge->m_owner->ResetCallStatus();

		return DOM_ExtensionUIItem::HandleItemAddedStatus(this_object, call_status, return_value);
	}

	DOM_CHECK_ARGUMENTS("o");

	ES_Object *properties = argv[0].value.object;
	DOM_Runtime *runtime = badge->GetRuntime();

	BOOL has_changed = FALSE;
	OP_STATUS status;
	OP_BOOLEAN result;
	ES_Value value;

	CALL_FAILED_IF_ERROR(result = runtime->GetName(properties, UNI_L("backgroundColor"), &value));
	if (result == OpBoolean::IS_TRUE)
	{
		has_changed = TRUE;
		UINT32 new_color;

		if (OpStatus::IsSuccess(status = ConstructUIColor(this_object, new_color, value, return_value, origining_runtime)))
			badge->m_background_color = new_color;
		else
			return (OpStatus::IsMemoryError(status) ? ES_NO_MEMORY : ES_EXCEPTION);
	}

	CALL_FAILED_IF_ERROR(result = runtime->GetName(properties, UNI_L("color"), &value));
	if (result == OpBoolean::IS_TRUE)
	{
		has_changed = TRUE;
		UINT32 new_color;

		if (OpStatus::IsSuccess(status = ConstructUIColor(this_object, new_color, value, return_value, origining_runtime)))
			badge->m_color = new_color;
		else
			return (OpStatus::IsMemoryError(status) ? ES_NO_MEMORY : ES_EXCEPTION);
	}

	CALL_FAILED_IF_ERROR(result = runtime->GetName(properties, UNI_L("display"), &value));
	if (result == OpBoolean::IS_TRUE && value.type == VALUE_STRING)
	{
		const uni_char *disp = value.value.string;
		BOOL is_none = FALSE;
		if ((is_none = uni_str_eq("none", disp)) || uni_str_eq("block", disp))
		{
			if (badge->m_displayed == is_none)
				has_changed = TRUE;
			badge->m_displayed = !is_none;
		}
		else
			has_changed = FALSE;
	}

	CALL_FAILED_IF_ERROR(result = runtime->GetName(properties, UNI_L("textContent"), &value));
	if (result == OpBoolean::IS_TRUE && value.type == VALUE_STRING)
	{
		has_changed = TRUE;
		RETURN_IF_ERROR(UniSetConstStr(badge->m_text_content, value.value.string));
	}

	if (has_changed)
	{
		CALL_INVALID_IF_ERROR(badge->m_owner->NotifyItemUpdate(origining_runtime, badge->m_owner));
		return (ES_SUSPEND | ES_RESTART);
	}
	else
		return ES_FAILED;
}

OP_STATUS
DOM_ExtensionUIBadge::Initialize(DOM_Object *this_object, ES_Object *properties, DOM_ExtensionUIItem *owner, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	m_owner = owner;
	m_background_color = DEFAULT_BADGE_BACKGROUND_COLOR_VALUE;
	m_color = DEFAULT_BADGE_COLOR_VALUE;
	m_displayed = DEFAULT_BADGE_DISPLAYED;
	m_text_content = NULL;

	OP_BOOLEAN result;
	ES_Value value;
	RETURN_IF_ERROR(result = origining_runtime->GetName(properties, UNI_L("backgroundColor"), &value));
	if (result == OpBoolean::IS_TRUE)
	{
		UINT32 new_color;
		RETURN_IF_ERROR(ConstructUIColor(this_object, new_color, value, return_value, origining_runtime));
		m_background_color = new_color;
	}

	RETURN_IF_ERROR(result = origining_runtime->GetName(properties, UNI_L("color"), &value));
	if (result == OpBoolean::IS_TRUE)
	{
		UINT32 new_color;
		RETURN_IF_ERROR(ConstructUIColor(this_object, new_color, value, return_value, origining_runtime));
		m_color = new_color;
	}

	RETURN_IF_ERROR(result = origining_runtime->GetName(properties, UNI_L("display"), &value));
	if (result == OpBoolean::IS_TRUE && value.type == VALUE_STRING)
	{
		const uni_char *disp = value.value.string;
		BOOL is_none = FALSE;
		if ((is_none = uni_str_eq("none", disp)) || uni_str_eq("block", disp))
			m_displayed = !is_none;
	}

	RETURN_IF_ERROR(result = origining_runtime->GetName(properties, UNI_L("textContent"), &value));
	if (result == OpBoolean::IS_TRUE)
	{
		if (value.type == VALUE_STRING)
			RETURN_IF_ERROR(UniSetConstStr(m_text_content, value.value.string));
		else if (value.type == VALUE_NUMBER)
		{
			char num_str_buf[32]; /* ARRAY OK 2010-11-09 sof */
			OpDoubleFormat::ToString(num_str_buf, value.value.number);
			RETURN_IF_ERROR(SetStr(const_cast<uni_char *&>(m_text_content), const_cast<const char *>(num_str_buf)));
		}
		else if (value.type == VALUE_BOOLEAN)
			RETURN_IF_ERROR(SetStr(const_cast<uni_char *&>(m_text_content), value.value.boolean ? "true" : "false"));
	}
	return OpStatus::OK;
}

DOM_ExtensionUIBadge::DOM_ExtensionUIBadge()
	: m_owner(NULL)
	, m_text_content(NULL)
	, m_displayed(DEFAULT_BADGE_DISPLAYED)
	, m_background_color(0)
	, m_color(0)
{
}

DOM_ExtensionUIBadge::~DOM_ExtensionUIBadge()
{
	OP_NEW_DBG("DOM_ExtensionUIBadge::~DOM_ExtensionUIBadge()", "extensions.dom");
	OP_DBG(("this: %p text_content: %s", this, m_text_content));

	OP_DELETEA(m_text_content);
}

/* virtual */ ES_GetState
DOM_ExtensionUIBadge::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch(property_name)
	{
	case OP_ATOM_display:
		DOMSetString(value, m_displayed ? UNI_L("block") : UNI_L("none"));
		return GET_SUCCESS;
	case OP_ATOM_textContent:
		DOMSetString(value, m_text_content ? m_text_content : DEFAULT_BADGE_TEXTCONTENT);
		return GET_SUCCESS;
	case OP_ATOM_backgroundColor:
		return DOMCanvasColorUtil::DOMSetCanvasColor(value, m_background_color);
	case OP_ATOM_color:
		return DOMCanvasColorUtil::DOMSetCanvasColor(value, m_color);
	default:
		return DOM_Object::GetName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_PutState
DOM_ExtensionUIBadge::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch(property_name)
	{
	case OP_ATOM_display:
		if (value->type == VALUE_BOOLEAN && m_displayed != static_cast<BOOL>(value->value.boolean))
		{
			m_displayed = value->value.boolean;
			PUT_FAILED_IF_ERROR_EXCEPTION(m_owner->NotifyItemUpdate(origining_runtime, m_owner), INVALID_STATE_ERR);
			DOMSetNull(value);
			return PUT_SUSPEND;
		}
		else if (value->type != VALUE_STRING)
			return PUT_NEEDS_STRING;
		else if (uni_str_eq(value->value.string, "block") || uni_str_eq(value->value.string, "none"))
		{
			m_displayed = uni_str_eq("block", value->value.string);
			PUT_FAILED_IF_ERROR_EXCEPTION(m_owner->NotifyItemUpdate(origining_runtime, m_owner), INVALID_STATE_ERR);
			DOMSetNull(value);
			return PUT_SUSPEND;
		}
		else
			return PUT_SUCCESS;

	case OP_ATOM_textContent:
		if (value->type != VALUE_STRING)
			return PUT_NEEDS_STRING;
		else if (!m_text_content || !uni_str_eq(value->value.string, m_text_content))
		{
			PUT_FAILED_IF_ERROR(UniSetConstStr(m_text_content, value->value.string));
			PUT_FAILED_IF_ERROR_EXCEPTION(m_owner->NotifyItemUpdate(origining_runtime, m_owner), INVALID_STATE_ERR);
			DOMSetNull(value);
			return PUT_SUSPEND;
		}
		else
			return PUT_SUCCESS;

	case OP_ATOM_backgroundColor:
		if (value->type == VALUE_STRING || value->type == VALUE_OBJECT)
		{
			UINT32 new_color;
			OP_STATUS status;
			ES_Value return_value;

			if (OpStatus::IsSuccess(status = ConstructUIColor(this, new_color, *value, &return_value, origining_runtime)))
			{
				m_background_color = new_color;
				PUT_FAILED_IF_ERROR_EXCEPTION(m_owner->NotifyItemUpdate(origining_runtime, m_owner), INVALID_STATE_ERR);
				DOMSetNull(value);
				return PUT_SUSPEND;
			}
			else
				return (OpStatus::IsMemoryError(status) ? PUT_NO_MEMORY : PUT_EXCEPTION);
		}
		else
			return PUT_NEEDS_STRING;

	case OP_ATOM_color:
		if (value->type == VALUE_STRING || value->type == VALUE_OBJECT)
		{
			UINT32 new_color;
			OP_STATUS status;
			ES_Value return_value;

			if (OpStatus::IsSuccess(status = ConstructUIColor(this, new_color, *value, &return_value, origining_runtime)))
			{
				m_color = new_color;
				PUT_FAILED_IF_ERROR_EXCEPTION(m_owner->NotifyItemUpdate(origining_runtime, m_owner), INVALID_STATE_ERR);
				DOMSetNull(value);
				return PUT_SUSPEND;
			}
			else
				return (OpStatus::IsMemoryError(status) ? PUT_NO_MEMORY : PUT_EXCEPTION);
		}
		else
			return PUT_NEEDS_STRING;

	default:
		return DOM_Object::PutName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_PutState
DOM_ExtensionUIBadge::PutNameRestart(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime, ES_Object *restart_object)
{
	switch (property_name)
	{
	case OP_ATOM_display:
	case OP_ATOM_textContent:
	case OP_ATOM_backgroundColor:
	case OP_ATOM_color:
		{
			m_owner->SetBlockedThread(NULL);
			m_owner->GetThreadListener()->Remove();
			OpExtensionUIListener::ItemAddedStatus call_status = m_owner->GetAddedCallStatus();
			m_owner->ResetCallStatus();

			return ConvertCallToPutName(DOM_ExtensionUIItem::HandleItemAddedStatus(this, call_status, value), value);
		}
	default:
		return DOM_Object::PutNameRestart(property_name, value, origining_runtime, restart_object);
	}
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_ExtensionUIBadge)
	DOM_FUNCTIONS_FUNCTION(DOM_ExtensionUIBadge, DOM_ExtensionUIBadge::update, "update", "o-")
DOM_FUNCTIONS_END(DOM_ExtensionUIBadge)

#endif // EXTENSION_SUPPORT
