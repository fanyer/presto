/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include <core/pch.h>

#ifdef DOM_JIL_API_SUPPORT

#include "modules/dom/src/domjil/utils/jilapplicationnames.h"
#include "modules/dom/src/domobj.h"

void
JILApplicationNames::ConstructL()
{
	m_jil_name_to_type[OpApplicationListener::APPLICATION_ALARM] = UNI_L("ALARM");
	m_jil_name_to_type[OpApplicationListener::APPLICATION_BROWSER] = UNI_L("BROWSER");
	m_jil_name_to_type[OpApplicationListener::APPLICATION_CALCULATOR] = UNI_L("CALCULATOR");
	m_jil_name_to_type[OpApplicationListener::APPLICATION_CALENDAR] = UNI_L("CALENDAR");
	m_jil_name_to_type[OpApplicationListener::APPLICATION_CAMERA] = UNI_L("CAMERA");
	m_jil_name_to_type[OpApplicationListener::APPLICATION_CONTACTS] = UNI_L("CONTACTS");
	m_jil_name_to_type[OpApplicationListener::APPLICATION_FILE_MANAGER] = UNI_L("FILES");
	m_jil_name_to_type[OpApplicationListener::APPLICATION_GAME_MANAGER] = UNI_L("GAMES");
	m_jil_name_to_type[OpApplicationListener::APPLICATION_MAIL] = UNI_L("MAIL");
	m_jil_name_to_type[OpApplicationListener::APPLICATION_MEDIA_PLAYER] = UNI_L("MEDIAPLAYER");
	m_jil_name_to_type[OpApplicationListener::APPLICATION_MESSAGING] = UNI_L("MESSAGING");
	m_jil_name_to_type[OpApplicationListener::APPLICATION_PHONE] = UNI_L("PHONECALL");
	m_jil_name_to_type[OpApplicationListener::APPLICATION_IMAGE_VIEWER] = UNI_L("PICTURES");
	m_jil_name_to_type[OpApplicationListener::APPLICATION_PROGRAM_MANAGER] = UNI_L("PROG_MANAGER");
	m_jil_name_to_type[OpApplicationListener::APPLICATION_SETTINGS] = UNI_L("SETTINGS");
	m_jil_name_to_type[OpApplicationListener::APPLICATION_TODO_TASKS] = UNI_L("TASKS");
	m_jil_name_to_type[OpApplicationListener::APPLICATION_WIDGET_MANAGER] = UNI_L("WIDGET_MANAGER");
	m_jil_name_to_type[OpApplicationListener::APPLICATION_TEXT_EDITOR] = UNI_L("TEXT_EDITOR");
}

const uni_char*
JILApplicationNames::TypeToJILPredefinedName(OpApplicationListener::ApplicationType type)
{
	return m_jil_name_to_type[type];
}

OP_STATUS
JILApplicationNames::JILPredefinedNameToType(const uni_char* name, OpApplicationListener::ApplicationType* app_type)
{
	OP_ASSERT(name);
	for (int i = 0; i < OpApplicationListener::APPLICATION_LAST; ++i)
	{
		if (uni_strcmp(m_jil_name_to_type[i], name) == 0)
		{
			*app_type = static_cast<OpApplicationListener::ApplicationType>(i);
			return OpStatus::OK;
		}
	}
	return OpStatus::ERR_NO_SUCH_RESOURCE;
}

OP_STATUS
JILApplicationNames::GetApplicationByJILName(const uni_char* name, OpApplicationListener::ApplicationType* app_type)
{
	OP_STATUS status = JILPredefinedNameToType(name, app_type);
	if (status == OpStatus::ERR_NO_SUCH_RESOURCE && m_custom_applications.get())
	{
		CustomApplicationDesc* app_desc;
		status = m_custom_applications->GetData(name, &app_desc);
		RETURN_VALUE_IF_ERROR(status, OpStatus::ERR_NO_SUCH_RESOURCE);
		OP_ASSERT(app_desc);
		*app_type = app_desc->type;
	}

	return status;
}

OP_STATUS
JILApplicationNames::GetApplicationParameterType(OpApplicationListener::ApplicationType app_type, JILApplicationNames::JILApplicationParameterType* param_type)
{
	if (!param_type)
		return OpStatus::ERR_NULL_POINTER;

	switch (app_type)
	{
		case OpApplicationListener::APPLICATION_ALARM:
		case OpApplicationListener::APPLICATION_CALCULATOR:
		case OpApplicationListener::APPLICATION_CALENDAR:
		case OpApplicationListener::APPLICATION_CAMERA:
		case OpApplicationListener::APPLICATION_CONTACTS:
		case OpApplicationListener::APPLICATION_PROGRAM_MANAGER:
		case OpApplicationListener::APPLICATION_SETTINGS:
		case OpApplicationListener::APPLICATION_TODO_TASKS:
		case OpApplicationListener::APPLICATION_WIDGET_MANAGER:
			*param_type = APP_PARAM_NONE;
			break;

		case OpApplicationListener::APPLICATION_BROWSER:
			*param_type = APP_PARAM_URL;
			break;

		case OpApplicationListener::APPLICATION_FILE_MANAGER:
		case OpApplicationListener::APPLICATION_IMAGE_VIEWER:
		case OpApplicationListener::APPLICATION_TEXT_EDITOR:
		case OpApplicationListener::APPLICATION_MEDIA_PLAYER:
			*param_type = APP_PARAM_PATH;
			break;

		case OpApplicationListener::APPLICATION_GAME_MANAGER:
		case OpApplicationListener::APPLICATION_MAIL:
		case OpApplicationListener::APPLICATION_MESSAGING:
		case OpApplicationListener::APPLICATION_PHONE:
			*param_type = APP_PARAM_OTHER;
			break;

		case OpApplicationListener::APPLICATION_CUSTOM:
			*param_type = APP_PARAM_UNKNOWN;
			break;

		default:
			return OpStatus::ERR_NO_SUCH_RESOURCE;

	}

	return OpStatus::OK;
}

OP_STATUS
JILApplicationNames::MakeJILApplicationTypesObject(ES_Object** new_app_types_obj, DOM_Runtime* runtime)
{
	RETURN_IF_ERROR(runtime->CreateNativeObjectObject(new_app_types_obj));

	for (int i = 0; i < OpApplicationListener::APPLICATION_LAST; ++i)
	{
		ES_Value app_val;
		DOM_Object::DOMSetString(&app_val, m_jil_name_to_type[i]);
		RETURN_IF_ERROR(runtime->PutName(*new_app_types_obj, m_jil_name_to_type[i], app_val, PROP_DONT_DELETE | PROP_READ_ONLY));
	}
	return OpStatus::OK;
}

#endif // DOM_JIL_API_SUPPORT
