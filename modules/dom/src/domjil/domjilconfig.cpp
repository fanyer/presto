/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef DOM_JIL_API_SUPPORT

#include "modules/dom/src/domjil/domjilconfig.h"
#include "modules/doc/frm_doc.h"
#include "modules/dom/src/domruntime.h"
#include "modules/windowcommander/src/WindowCommander.h"

#include "modules/device_api/SystemResourceSetter.h"

/* static */ OP_STATUS
DOM_JILConfig::Make(DOM_JILConfig*& new_jil_config, DOM_Runtime* runtime)
{
	OP_ASSERT(runtime);
	new_jil_config = OP_NEW(DOM_JILConfig, ());
	RETURN_IF_ERROR(DOMSetObjectRuntime(new_jil_config, runtime, runtime->GetPrototype(DOM_Runtime::JIL_CONFIG_PROTOTYPE), "Config"));
	return OpStatus::OK;
}

/* virtual */ void
DOM_JILConfig::GCTrace()
{
	DOM_JILObject::GCTrace();
}

/* virtual */ ES_GetState
DOM_JILConfig::InternalGetName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value)
{
	switch (property_atom)
	{
		case OP_ATOM_msgRingtoneVolume:
		case OP_ATOM_ringtoneVolume:
		{
			if (value)
			{
				double volume;
				OpSystemInfo::RingtoneType ringtone_type = property_atom == OP_ATOM_msgRingtoneVolume ? OpSystemInfo::RINGTONE_MSG : OpSystemInfo::RINGTONE_CALL;
				GET_FAILED_IF_ERROR(g_op_system_info->GetRingtoneVolume(ringtone_type, &volume));
				// internal API uses values 0-1.0 jil uses 0-10.0
				DOMSetNumber(value, volume * 10.0);
			}
			return GET_SUCCESS;
		}
		case OP_ATOM_vibrationSetting:
		{
			if (value)
			{
				BOOL vibration_enabled;
				GET_FAILED_IF_ERROR(g_op_system_info->GetVibrationEnabled(&vibration_enabled));
				TempBuffer* buffer = GetEmptyTempBuf();
				// This is by the spec even though it is illogical and should have been boolean
				// Work ongoing to convinvce JIL to change it to boolean
				GET_FAILED_IF_ERROR(buffer->Append(vibration_enabled ? UNI_L("ON"): UNI_L("OFF")));
				DOMSetString(value, buffer);
			}
			return GET_SUCCESS;
		}
	}
	return GET_FAILED;
}

/* virtual */ ES_PutState
DOM_JILConfig::InternalPutName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value)
{
	switch (property_atom)
	{
		case OP_ATOM_msgRingtoneVolume:
		case OP_ATOM_ringtoneVolume:
		case OP_ATOM_vibrationSetting:
			return PUT_SUCCESS;
	}
	return PUT_FAILED;
}

/* static */ int
DOM_JILConfig::setAsWallpaper(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_JIL_CONFIG);
	DOM_CHECK_OR_RESTORE_ARGUMENTS_JIL("s");
	const uni_char* wallpaper_file_path = argv[0].value.string;
	OP_ASSERT(wallpaper_file_path);
	OpString wallpaper_file_path_buffer;
	if (DOM_CallState::GetPhaseFromESValue(return_value) < DOM_CallState::PHASE_EXECUTION_0)
	{
		// JIL accepts 2 forms of parameters here file:// urls and paths, which is
		// different than usual use of urls,where string without a schema is
		// relative to document and thats why we need this hack.
		if (uni_strncmp(wallpaper_file_path, UNI_L("file://"), 7) != 0)
		{
			CALL_FAILED_IF_ERROR(wallpaper_file_path_buffer.AppendFormat(UNI_L("file://%s"), wallpaper_file_path));
			wallpaper_file_path = wallpaper_file_path_buffer.CStr();
		}
	}

	DOM_SuspendingCall call(this_object, argv, argc, return_value, return_value, origining_runtime, DOM_CallState::PHASE_EXECUTION_0);
	NEW_SUSPENDING_CALLBACK(JILUtils::SetSystemResourceCallbackSuspendingImpl, callback, return_value, call, ());
	OpMemberFunctionObject5<SystemResourceSetter, const uni_char*, SystemResourceSetter::ResourceType, const uni_char* , SystemResourceSetter::SetSystemResourceCallback* , FramesDocument*>
		function(g_device_api_system_resource_setter, &SystemResourceSetter::SetSystemResource, wallpaper_file_path, SystemResourceSetter::DESKTOP_IMAGE, NULL, callback, origining_runtime->GetFramesDocument());
	DOM_SUSPENDING_CALL(call, function, JILUtils::SetSystemResourceCallbackSuspendingImpl, callback);
	OP_ASSERT(callback);

	CALL_FAILED_IF_ERROR_WITH_HANDLER(callback->GetErrorCode(), DOM_JILConfig::HandleSetSystemResourceError);
	return ES_FAILED;
}

/* static */ int
DOM_JILConfig::setDefaultRingtone(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_JIL_CONFIG);
	DOM_CHECK_OR_RESTORE_ARGUMENTS_JIL("s");

	const uni_char* ringtone_file_path = argv[0].value.string;
	OP_ASSERT(ringtone_file_path);
	OpString ringtone_file_path_buffer;
	if (DOM_CallState::GetPhaseFromESValue(return_value) < DOM_CallState::PHASE_EXECUTION_0)
	{
		// JIL accepts 2 forms of parameters here file:// urls and paths, which is
		// different than usual use of urls,where string without a schema is
		// relative to document and thats why we need this hack.
		if (uni_strncmp(ringtone_file_path, UNI_L("file://"), 7) != 0)
		{
			CALL_FAILED_IF_ERROR(ringtone_file_path_buffer.AppendFormat(UNI_L("file://%s"), ringtone_file_path));
			ringtone_file_path = ringtone_file_path_buffer.CStr();
		}
	}

	DOM_SuspendingCall call(this_object, argv, argc, return_value, return_value, origining_runtime, DOM_CallState::PHASE_EXECUTION_0);
	NEW_SUSPENDING_CALLBACK(JILUtils::SetSystemResourceCallbackSuspendingImpl, callback, return_value, call, ());
	OpMemberFunctionObject5<SystemResourceSetter, const uni_char*, SystemResourceSetter::ResourceType, const uni_char* , SystemResourceSetter::SetSystemResourceCallback* , FramesDocument*>
		function(g_device_api_system_resource_setter, &SystemResourceSetter::SetSystemResource, ringtone_file_path, SystemResourceSetter::DEFAULT_RINGTONE, NULL, callback, origining_runtime->GetFramesDocument());
	DOM_SUSPENDING_CALL(call, function, JILUtils::SetSystemResourceCallbackSuspendingImpl, callback);
	OP_ASSERT(callback);

	CALL_FAILED_IF_ERROR_WITH_HANDLER(callback->GetErrorCode(), DOM_JILConfig::HandleSetSystemResourceError);
	return ES_FAILED;
}

/* static */ int
DOM_JILConfig::HandleSetSystemResourceError(OP_SYSTEM_RESOURCE_SETTER_STATUS error_code, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	switch (error_code)
	{
	case OpStatus::ERR_NO_MEMORY:
		return ES_NO_MEMORY;
	case SystemResourceSetter::Status::ERR_RESOURCE_LOADING_FAILED:
		return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, origining_runtime, UNI_L("Could not load specified resource"));
	case SystemResourceSetter::Status::ERR_RESOURCE_FORMAT_UNSUPPORTED:
		return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, origining_runtime, UNI_L("Resource format unsupported"));
	case SystemResourceSetter::Status::ERR_RESOURCE_SAVE_OPERATION_FAILED:
		return CallException(DOM_Object::JIL_UNKNOWN_ERR, return_value, origining_runtime, UNI_L("Disk write error"));
	case OpStatus::ERR_NOT_SUPPORTED:
		return CallException(DOM_Object::JIL_UNSUPPORTED_ERR, return_value, origining_runtime);
	case OpStatus::ERR_FILE_NOT_FOUND:
	case OpStatus::ERR_NO_SUCH_RESOURCE:
		return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, origining_runtime, UNI_L("File not found"));
	case OpStatus::ERR_NO_DISK:
		return CallException(DOM_Object::JIL_UNKNOWN_ERR, return_value, origining_runtime, UNI_L("Not enough disk space"));
	case OpStatus::ERR_NO_ACCESS:
		return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, origining_runtime, UNI_L("No access to file"));
	default:
		return CallException(DOM_Object::JIL_UNKNOWN_ERR, return_value, origining_runtime, UNI_L("Invalid Parameter"));
	}
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_JILConfig)
	DOM_FUNCTIONS_WITH_SECURITY_RULE_A1(DOM_JILConfig, DOM_JILConfig::setAsWallpaper,     "setAsWallpaper",     "s-", "Config.setAsWallpaper", 0)
	DOM_FUNCTIONS_WITH_SECURITY_RULE_A1(DOM_JILConfig, DOM_JILConfig::setDefaultRingtone, "setDefaultRingtone", "s-", "Config.setDefaultRingtone", 0)
DOM_FUNCTIONS_END(DOM_JILConfig)

#endif // DOM_JIL_API_SUPPORT
