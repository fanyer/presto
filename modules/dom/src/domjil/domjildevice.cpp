/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef DOM_JIL_API_SUPPORT
#include "modules/dom/src/domjil/domjildevice.h"

#include "modules/doc/frm_doc.h"
#include "modules/dom/src/domruntime.h"

#include "modules/ecmascript/ecmascript.h"

#include "modules/ecmascript_utils/esasyncif.h"

#include "modules/device_api/jil/JILFolderLister.h"

#include "modules/dom/src/domjil/domjilaccountinfo.h"
#include "modules/dom/src/domjil/utils/jilutils.h"
#include "modules/dom/src/domjil/utils/jilapplicationnames.h"
#include "modules/dom/src/domjil/domjilconfig.h"
#include "modules/dom/src/domjil/domjildatanetworkinfo.h"
#include "modules/dom/src/domjil/domjildeviceinfo.h"
#include "modules/dom/src/domjil/domjildevicestateinfo.h"
#include "modules/dom/src/domjil/domjildatanetworkinfo.h"
#include "modules/dom/src/domjil/domjilfile.h"
#include "modules/dom/src/domjil/domjilradioinfo.h"
#include "modules/dom/src/domjil/domjilpowerinfo.h"

#include "modules/dom/src/domsuspendcallback.h"

#include "modules/formats/uri_escape.h"

#include "modules/gadgets/OpGadget.h"
#include "modules/gadgets/OpGadgetManager.h"

#include "modules/windowcommander/OpWindowCommander.h"
#include "modules/windowcommander/src/WindowCommander.h"

#ifdef USE_OP_CLIPBOARD
#include "modules/pi/OpClipboard.h"
#endif // USE_OP_CLIPBOARD

/** @short Callback for refreshing cache of custom installed applications.
 */
class DOM_JILGetCustomInstalledApplicationsCallback : public OpApplicationListener::GetInstalledApplicationsCallback
{
public:

	virtual OP_STATUS OnKnownApplication(OpApplicationListener::ApplicationType type)
	{
		return OpStatus::OK;
	}

	virtual OP_STATUS OnCustomApplication(OpApplicationListener::ApplicationType type, const uni_char* path)
	{
		return m_custom_apps.AddApplication(type, path);
	}

	virtual void OnFinished()
	{
		m_custom_apps.Give(g_DOM_jilUtils->GetApplicationNames());
		OP_DELETE(this);
	}

	virtual void OnFailed(OP_STATUS)
	{
		OP_DELETE(this);
	}

private:
	CustomApplicationsBuilder m_custom_apps;
};

/** @short Callback for collecting installed applications' names.
 *
 * Application names are inserted into an EcmaScript array object.
 * The "well-known" applications receive names that are equal to
 * constants in Widget.ApplicationTypes ES object, the others
 * are named with their executable paths.
 */
class DOM_JILGetInstalledApplicationsCallback : public DOM_SuspendCallback<OpApplicationListener::GetInstalledApplicationsCallback>
{
public:

	/** Constructor.
	 *
	 * @param runtime The runtime in which the result is to be constructed.
	 */
	DOM_JILGetInstalledApplicationsCallback(DOM_Runtime* runtime)
		: m_runtime(runtime)
		, m_result_index(0)
		, m_result_array(NULL)
	{
	}

	~DOM_JILGetInstalledApplicationsCallback()
	{
		if (m_result_array)
			m_runtime->Unprotect(m_result_array);
	}

	virtual OP_STATUS Construct()
	{
		RETURN_IF_ERROR(m_runtime->CreateNativeArrayObject(&m_result_array));
		BOOL is_protected = m_runtime->Protect(m_result_array);
		if (!is_protected)
		{
			m_result_array = NULL;
			return OpStatus::ERR;
		}
		return OpStatus::OK;
	}

	virtual OP_STATUS OnKnownApplication(OpApplicationListener::ApplicationType type)
	{
		const uni_char* name = g_DOM_jilUtils->GetApplicationNames()->TypeToJILPredefinedName(type);
		OP_ASSERT(name);
		RETURN_IF_ERROR(InsertResult(name));
		return OpStatus::OK;
	}

	virtual OP_STATUS OnCustomApplication(OpApplicationListener::ApplicationType type, const uni_char* path)
	{
		RETURN_IF_ERROR(m_custom_apps.AddApplication(type, path));
		return InsertResult(path);
	}

	virtual void OnFinished()
	{
		m_custom_apps.Give(g_DOM_jilUtils->GetApplicationNames());
		OnSuccess();
	}

	virtual void OnFailed(OP_STATUS error)
	{
		DOM_SuspendCallbackBase::OnFailed(error);
	}

	/** Provide access to the result.
	 *
	 * @return pointer provided to this object's constructor.
	 */
	ES_Object* GetResult()
	{
		return m_result_array;
	}

private:
	OP_STATUS InsertResult(const uni_char* value)
	{
		ES_Value es_string;
		es_string.type = VALUE_STRING;
		es_string.value.string = value;
		RETURN_IF_ERROR(m_runtime->PutIndex(m_result_array, m_result_index, es_string));
		++m_result_index;
		return OpStatus::OK;
	}

	DOM_Runtime* m_runtime;
	unsigned int m_result_index;
	ES_Object* m_result_array;
	CustomApplicationsBuilder m_custom_apps;
};


class DOM_JILPositionInfo_Constructor : public DOM_BuiltInConstructor
{
public:
	DOM_JILPositionInfo_Constructor() : DOM_BuiltInConstructor(DOM_Runtime::JIL_POSITIONINFO_PROTOTYPE) { }

	virtual int Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* es_origining_runtime)
	{
		DOM_Runtime* origining_runtime = static_cast<DOM_Runtime*>(es_origining_runtime);
		ES_Object* new_file_obj;
		CALL_FAILED_IF_ERROR(DOM_JILDeviceStateInfo::MakePositionInfoObject(&new_file_obj, origining_runtime, NULL));
		DOMSetObject(return_value, new_file_obj);
		return ES_VALUE;
	}
};

/* static */
OP_STATUS DOM_JILDevice::Make(DOM_JILDevice*& device, DOM_Runtime* runtime)
{
	device = OP_NEW(DOM_JILDevice, ());

	RETURN_IF_ERROR(DOMSetObjectRuntime(device, runtime, runtime->GetPrototype(DOM_Runtime::JIL_DEVICE_PROTOTYPE), "Device"));
	if (g_DOM_jilUtils->IsJILApiRequested(JILFeatures::JIL_API_DATANETWORKINFO, runtime))
	{
		DOM_JILDataNetworkInfo* data_network_info;
		RETURN_IF_ERROR(DOM_JILDataNetworkInfo::Make(data_network_info, runtime));
		ES_Value data_network_info_val;
		DOMSetObject(&data_network_info_val, data_network_info);
		RETURN_IF_ERROR(runtime->PutName(device->GetNativeObject(), UNI_L("DataNetworkInfo"), data_network_info_val, PROP_READ_ONLY | PROP_DONT_DELETE));
	}

	if (g_DOM_jilUtils->IsJILApiRequested(JILFeatures::JIL_API_DEVICESTATEINFO, runtime))
	{
		DOM_JILDeviceStateInfo* device_state_info;
		RETURN_IF_ERROR(DOM_JILDeviceStateInfo::Make(device_state_info, runtime));
		ES_Value device_state_info_val;
		DOMSetObject(&device_state_info_val, device_state_info);
		RETURN_IF_ERROR(runtime->PutName(device->GetNativeObject(), UNI_L("DeviceStateInfo"), device_state_info_val, PROP_READ_ONLY | PROP_DONT_DELETE));
	}

	if (g_DOM_jilUtils->IsJILApiRequested(JILFeatures::JIL_API_DEVICEINFO, runtime))
	{
		DOM_JILDeviceInfo* device_info;
		RETURN_IF_ERROR(DOM_JILDeviceInfo::Make(device_info, runtime));
		ES_Value device_info_val;
		DOMSetObject(&device_info_val, device_info);
		RETURN_IF_ERROR(runtime->PutName(device->GetNativeObject(), UNI_L("DeviceInfo"), device_info_val, PROP_READ_ONLY | PROP_DONT_DELETE));
	}

	if (g_DOM_jilUtils->IsJILApiRequested(JILFeatures::JIL_API_ACCOUNTINFO, runtime))
	{
		DOM_JILAccountInfo* account;
		RETURN_IF_ERROR(DOM_JILAccountInfo::Make(account, runtime));
		ES_Value account_val;
		DOMSetObject(&account_val, account);
		RETURN_IF_ERROR(runtime->PutName(device->GetNativeObject(), UNI_L("AccountInfo"), account_val, PROP_READ_ONLY | PROP_DONT_DELETE));
	}

	if (g_DOM_jilUtils->IsJILApiRequested(JILFeatures::JIL_API_APPLICATIONTYPES, runtime))
	{
		ES_Object* es_application_types;
		RETURN_IF_ERROR(g_DOM_jilUtils->GetApplicationNames()->MakeJILApplicationTypesObject(&es_application_types, runtime));
		ES_Value application_types_val;
		DOMSetObject(&application_types_val, es_application_types);
		RETURN_IF_ERROR(runtime->PutName(device->GetNativeObject(), UNI_L("ApplicationTypes"), application_types_val, PROP_READ_ONLY | PROP_DONT_DELETE));
	}

	if (g_DOM_jilUtils->IsJILApiRequested(JILFeatures::JIL_API_RADIOINFO, runtime))
	{
		DOM_JILRadioInfo* radio_info;
		RETURN_IF_ERROR(DOM_JILRadioInfo::Make(radio_info, runtime));
		ES_Value radio_info_val;
		DOMSetObject(&radio_info_val, radio_info);
		RETURN_IF_ERROR(runtime->PutName(device->GetNativeObject(),UNI_L("RadioInfo"), radio_info_val, PROP_READ_ONLY | PROP_DONT_DELETE));
	}

	if (g_DOM_jilUtils->IsJILApiRequested(JILFeatures::JIL_API_POWERINFO, runtime))
	{
		DOM_JILPowerInfo* power_info;
		RETURN_IF_ERROR(DOM_JILPowerInfo::Make(power_info, runtime));
		ES_Value power_info_val;
		DOMSetObject(&power_info_val, power_info);
		RETURN_IF_ERROR(runtime->PutName(device->GetNativeObject(), UNI_L("PowerInfo"), power_info_val, PROP_READ_ONLY | PROP_DONT_DELETE));
	}

	OpString engine_name, engine_provider, engine_version;
	RETURN_IF_LEAVE(
		if (g_DOM_jilUtils->IsJILApiRequested(JILFeatures::JIL_API_FILE, runtime))
			device->PutFunctionL("File", OP_NEW(DOM_JILFile_Constructor, ()), "File", NULL);

		g_prefsManager->GetPreferenceL("JIL API", "Widget Engine Name", engine_name);
		g_prefsManager->GetPreferenceL("JIL API", "Widget Engine Provider", engine_provider);
		g_prefsManager->GetPreferenceL("JIL API", "Widget Engine Version", engine_version);
		if (g_DOM_jilUtils->IsJILApiRequested(JILFeatures::JIL_API_FILE, runtime))
			device->PutFunctionL("File", OP_NEW(DOM_JILFile_Constructor, ()), "File", NULL);

		if (g_DOM_jilUtils->IsJILApiRequested(JILFeatures::JIL_API_POSITIONINFO, runtime))
			RETURN_IF_LEAVE(device->PutFunctionL("PositionInfo", OP_NEW(DOM_JILPositionInfo_Constructor, ()), "PositionInfo", NULL));
	);

	RETURN_IF_ERROR(device->PutStringConstant(UNI_L("widgetEngineName"), engine_name.CStr()));
	RETURN_IF_ERROR(device->PutStringConstant(UNI_L("widgetEngineProvider"), engine_provider.CStr()));
	RETURN_IF_ERROR(device->PutStringConstant(UNI_L("widgetEngineVersion"), engine_version.CStr()));

	OpApplicationListener* app_listener = runtime->GetFramesDocument()->GetWindow()->GetWindowCommander()->GetApplicationListener();
	OP_ASSERT(app_listener);
	DOM_JILGetCustomInstalledApplicationsCallback* callback = OP_NEW(DOM_JILGetCustomInstalledApplicationsCallback, ());
	RETURN_OOM_IF_NULL(callback);
	app_listener->GetInstalledApplications(callback);

	return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_JILDevice::AddInstalledWidgetIds(ES_Object* result_array, DOM_Runtime* runtime)
{
	UINT32 gadget_count = g_gadget_manager->NumGadgets();
	ES_Value array_length;

	OP_BOOLEAN length_retrieved = runtime->GetName(result_array, UNI_L("length"), &array_length);
	RETURN_IF_ERROR(length_retrieved);
	if (length_retrieved == OpBoolean::IS_FALSE || array_length.type != VALUE_NUMBER)
		return OpStatus::ERR;

	OP_ASSERT(array_length.value.number >= 0);
	// We're never going to have as many apps and widgets as even a fraction of UINT_MAX.
	unsigned int array_index = static_cast<unsigned int>(array_length.value.number > UINT_MAX ? UINT_MAX : array_length.value.number);

	for (UINT32 i = 0; i < gadget_count; ++i)
	{
		OpGadget* gadget = g_gadget_manager->GetGadget(i);

		OpString gadget_name;
		RETURN_IF_ERROR(gadget->GetGadgetName(gadget_name));

		OpString gadget_uri;
		RETURN_IF_ERROR(gadget->GetGadgetUrl(gadget_uri, FALSE, TRUE));
		if (gadget_name.CStr())
		{
			OpString gadget_name_escaped;
			RETURN_IF_ERROR(UriEscape::AppendEscaped(gadget_name_escaped, gadget_name.CStr(), UriEscape::AllUnsafe));

			RETURN_IF_ERROR(gadget_uri.AppendFormat(UNI_L("?wname=%s"), gadget_name_escaped.CStr()));
		}
		else
			RETURN_IF_ERROR(gadget_uri.Append(UNI_L("?wname=")));


		ES_Value gadget_uri_value;
		DOMSetString(&gadget_uri_value, gadget_uri.CStr());
		runtime->PutIndex(result_array, array_index, gadget_uri_value);
		++array_index;
	}

	return OpStatus::OK;
}

/* static */ int
DOM_JILDevice::getAvailableApplications(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_JIL_DEVICE);
	DOM_CHECK_OR_RESTORE_ARGUMENTS_JIL("");

	OpApplicationListener* app_listener = origining_runtime->GetFramesDocument()->GetWindow()->GetWindowCommander()->GetApplicationListener();

	DOM_SuspendingCall call(this_object, argv, argc, return_value, return_value, origining_runtime, DOM_CallState::PHASE_EXECUTION_0);

	NEW_SUSPENDING_CALLBACK(DOM_JILGetInstalledApplicationsCallback, callback, return_value, call, (origining_runtime));
	OpMemberFunctionObject1<OpApplicationListener, OpApplicationListener::GetInstalledApplicationsCallback*>
		function(app_listener, &OpApplicationListener::GetInstalledApplications, callback);

	DOM_SUSPENDING_CALL(call, function, DOM_JILGetInstalledApplicationsCallback, callback);

	CALL_FAILED_IF_ERROR_WITH_HANDLER(callback->GetErrorCode(), HandleApplicationError);

	ES_Object* result_array = callback->GetResult();
	AddInstalledWidgetIds(result_array, origining_runtime);
	DOMSetObject(return_value, result_array);
	return ES_VALUE;
}

/* static */ int
DOM_JILDevice::launchApplication(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_JIL_DEVICE);
	DOM_CHECK_ARGUMENTS_JIL("s|S");

	const uni_char* application_name = argv[0].value.string;
	const uni_char* parameter = (argc >= 2 && argv[1].type == VALUE_STRING) ? argv[1].value.string : NULL; // it can be also null

	if (uni_strni_eq(application_name, UNI_L("widget://"), 9))
	{
		int wuid_end_index = static_cast<int>(uni_strcspn(application_name, UNI_L("?")));

		OpString gadget_uri;
		CALL_FAILED_IF_ERROR(gadget_uri.Set(application_name, wuid_end_index));

		OpGadget* gadget = g_gadget_manager->FindGadget(GADGET_FIND_BY_URL_WITHOUT_INDEX, gadget_uri.CStr());
		if (gadget)
			CALL_FAILED_IF_ERROR(g_gadget_manager->OpenGadget(gadget));
		else
		{
			OpString message;
			CALL_FAILED_IF_ERROR(message.AppendFormat(UNI_L("Unknown widget URL: %s"), application_name));
			return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, origining_runtime, message.CStr());
		}
	}
	else
	{
		OpApplicationListener::ApplicationType application_type;
		OpApplicationListener* app_listener = origining_runtime->GetFramesDocument()->GetWindow()->GetWindowCommander()->GetApplicationListener();

		CALL_FAILED_IF_ERROR_WITH_HANDLER(g_DOM_jilUtils->GetApplicationNames()->GetApplicationByJILName(application_name, &application_type), HandleApplicationError);

		OpString translated_parameter;
		if (parameter && *parameter)
		{
			JILApplicationNames::JILApplicationParameterType param_type;
			CALL_FAILED_IF_ERROR_WITH_HANDLER(g_DOM_jilUtils->GetApplicationNames()->GetApplicationParameterType(application_type, &param_type), HandleApplicationError);

			if (param_type == JILApplicationNames::APP_PARAM_PATH)
			CALL_FAILED_IF_ERROR_WITH_HANDLER(g_DAPI_jil_fs_mgr->JILToSystemPath(parameter, translated_parameter), HandleFileOperationError);
			else if (param_type == JILApplicationNames::APP_PARAM_URL)
			{
				OpGadget* gadget = g_DOM_jilUtils->GetGadgetFromRuntime(origining_runtime);
				OP_ASSERT(gadget);
				URL tmp_url = g_url_api->GetURL(parameter, gadget->UrlContextId());
				if (tmp_url.Type() == URL_FILE)
				{
					if (!g_DAPI_jil_fs_mgr->IsFileAccessAllowed(tmp_url))
					{
						this_object->CreateJILException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, origining_runtime);
						return ES_EXCEPTION;
					}
					else
						g_DAPI_jil_fs_mgr->JILToSystemPath(tmp_url.GetAttribute(URL::KUniPath), translated_parameter);
				}
			}
		}

		const uni_char* translated_uni_parameter = translated_parameter.CStr();
		CALL_FAILED_IF_ERROR_WITH_HANDLER(app_listener->ExecuteApplication(application_type, parameter ? 1 : 0, translated_uni_parameter ? &translated_uni_parameter : &parameter), HandleApplicationError);
	}
	return ES_FAILED;
}

class JILDeviceFilesFoundListenerSync : public DOM_SuspendCallback<FilesFoundListener>
{
private:
	JILFileFinder m_file_finder;
	OpVector<OpString>* m_result;
public:
	JILDeviceFilesFoundListenerSync()
	: m_result(NULL)
	{
	}

	~JILDeviceFilesFoundListenerSync() {}

	void OnFilesFound(OpVector<OpString>* file_list)
	{
		m_result = file_list;
		OnSuccess();
	}

	void OnError(OP_STATUS err)
	{
		OnFailed(err);
	}

	JILFileFinder* GetFileFinder() { return &m_file_finder; }
	OpVector<OpString>* GetResult() { return m_result; }
};

/* static */ int
DOM_JILDevice::getDirectoryFileNames(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_JIL_DEVICE);
	DOM_CHECK_OR_RESTORE_ARGUMENTS_JIL("s");

	if (!g_DOM_jilUtils->RuntimeHasFilesystemAccess(origining_runtime))
	{
		this_object->CreateJILException(DOM_Object::JIL_SECURITY_ERR, return_value, origining_runtime);
		return ES_EXCEPTION;
	}

	// This can be achieved using JILFolderLister as well however JIlFileFinder is async & yielding so it will not hang Opera when listing lots of entries
	// as using JILFolderLister would do.

	DOM_SuspendingCall call(this_object, argv, argc, return_value, return_value, origining_runtime, DOM_CallState::PHASE_EXECUTION_0);
	const uni_char* path = argv[0].value.string;
	NEW_SUSPENDING_CALLBACK(JILDeviceFilesFoundListenerSync, callback, return_value, call, ());
	JILFileFinder* file_finder = NULL;
	if (callback)
		file_finder = callback->GetFileFinder();

	OpMemberFunctionObject5<JILFileFinder, const uni_char*, const uni_char*, FilesFoundListener*, BOOL, OpFileInfo*>
		function(file_finder, &JILFileFinder::Find, path, UNI_L("*"), callback, FALSE, NULL);

	DOM_SUSPENDING_CALL(call, function, JILDeviceFilesFoundListenerSync, callback);

	CALL_FAILED_IF_ERROR_WITH_HANDLER(callback->GetErrorCode(), HandleFileOperationError);
	ES_Object* directory_names_array = NULL;
	CALL_FAILED_IF_ERROR_WITH_HANDLER(origining_runtime->CreateNativeArrayObject(&directory_names_array), HandleFileOperationError);
	ES_Value val;
	OpVector<OpString>* results = callback->GetResult();
	for (UINT32 cnt = 0; results && cnt < results->GetCount(); ++cnt)
	{
		OP_ASSERT(results->Get(cnt));
		OpString* elem = results->Get(cnt);
		int path_len = static_cast<int>(uni_strlen(path));
		if (elem->CStr() && elem->CStr()[path_len] == JILPATHSEPCHAR)
			++path_len;
		DOM_Object::DOMSetString(&val, elem->CStr() + path_len);
		CALL_FAILED_IF_ERROR_WITH_HANDLER(origining_runtime->PutIndex(directory_names_array, cnt, val), HandleFileOperationError);
	}

	DOMSetObject(return_value, directory_names_array);
	return ES_VALUE;
}

/* static */ int
DOM_JILDevice::getFile(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_JIL_DEVICE);
	DOM_CHECK_ARGUMENTS_JIL("s");

	const uni_char* file_path = argv[0].value.string;

	if (!g_DOM_jilUtils->RuntimeHasFilesystemAccess(origining_runtime))
	{
		this_object->CreateJILException(DOM_Object::JIL_SECURITY_ERR, return_value, origining_runtime);
		return ES_EXCEPTION;
	}

	DOM_JILFile* new_file_object;
	CALL_FAILED_IF_ERROR_WITH_HANDLER(DOM_JILFile::Make(new_file_object, origining_runtime, file_path), HandleFileOperationError);
	DOMSetObject(return_value, new_file_object);
	return ES_VALUE;
}

/* static */ int
DOM_JILDevice::copyFile(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	// TODO: Change copying to asynchronous if synchronous is causing troubles.
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_JIL_DEVICE);
	DOM_CHECK_ARGUMENTS_JIL("ss");

	if (!g_DOM_jilUtils->RuntimeHasFilesystemAccess(origining_runtime))
	{
		this_object->CreateJILException(DOM_Object::JIL_SECURITY_ERR, return_value, origining_runtime);
		return ES_EXCEPTION;
	}

	const uni_char* jil_from = argv[0].value.string;
	const uni_char* jil_to = argv[1].value.string;
	OpString system_from;
	OpString system_to;

	CALL_FAILED_IF_ERROR_WITH_HANDLER(g_DAPI_jil_fs_mgr->JILToSystemPath(jil_from, system_from), HandleFileOperationError);
	CALL_FAILED_IF_ERROR_WITH_HANDLER(g_DAPI_jil_fs_mgr->JILToSystemPath(jil_to, system_to), HandleFileOperationError);

	OpFile from_file;
	OpFile to_file;
	CALL_FAILED_IF_ERROR_WITH_HANDLER(from_file.Construct(system_from), HandleFileOperationError);
	CALL_FAILED_IF_ERROR_WITH_HANDLER(to_file.Construct(system_to), HandleFileOperationError);
	BOOL exists;
	CALL_FAILED_IF_ERROR_WITH_HANDLER(to_file.Exists(exists), HandleFileOperationError);

	if (exists)
	{
		this_object->CreateJILException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, origining_runtime, UNI_L("The destination file already exists"));
		return ES_EXCEPTION;
	}

	CALL_FAILED_IF_ERROR_WITH_HANDLER(from_file.Exists(exists), HandleFileOperationError);
	if (!exists)
	{
		this_object->CreateJILException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, origining_runtime, UNI_L("The source file does not exist"));
		return ES_EXCEPTION;
	}

	OpFileInfo::Mode from_file_mode;
	CALL_FAILED_IF_ERROR_WITH_HANDLER(from_file.GetMode(from_file_mode), HandleFileOperationError);
	if (from_file_mode == OpFileInfo::DIRECTORY)
		CALL_FAILED_IF_ERROR_WITH_HANDLER(to_file.MakeDirectory(), HandleFileOperationError);
	else
		CALL_FAILED_IF_ERROR_WITH_HANDLER(to_file.CopyContents(&from_file, FALSE), HandleFileOperationError);

	DOMSetBoolean(return_value, TRUE);

	return ES_VALUE;
}

/* static */ int
DOM_JILDevice::moveFile(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	/* Doing this way is not efficient especially when only renaming. Consider extending OpFile API with move
	 * operation taking care of all the details
	 */
	int result = copyFile(this_object, argv, argc, return_value, origining_runtime);
	if (result == ES_VALUE)
		result = deleteFile(this_object, argv, 1, return_value, origining_runtime);

	return result;
}

/* static */ int
DOM_JILDevice::deleteFile(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_JIL_DEVICE);
	DOM_CHECK_ARGUMENTS_JIL("s");

	if (!g_DOM_jilUtils->RuntimeHasFilesystemAccess(origining_runtime))
	{
		this_object->CreateJILException(DOM_Object::JIL_SECURITY_ERR, return_value, origining_runtime);
		return ES_EXCEPTION;
	}

	const uni_char* jil_file = argv[0].value.string;
	OpString system_file;

	CALL_FAILED_IF_ERROR_WITH_HANDLER(g_DAPI_jil_fs_mgr->JILToSystemPath(jil_file, system_file), HandleFileOperationError);

	OpFile file;
	CALL_FAILED_IF_ERROR_WITH_HANDLER(file.Construct(system_file), HandleFileOperationError);
	BOOL exists;
	CALL_FAILED_IF_ERROR_WITH_HANDLER(file.Exists(exists), HandleDeleteFileOperationError);
	if (!exists)
		return HandleDeleteFileOperationError(OpStatus::ERR_FILE_NOT_FOUND, return_value, origining_runtime);
	CALL_FAILED_IF_ERROR_WITH_HANDLER(file.Delete(FALSE), HandleDeleteFileOperationError);

	DOMSetBoolean(return_value, TRUE);

	return ES_VALUE;
}

DOM_JILDevice::JILDeviceFilesFoundListenerAsync::SearchParams::~SearchParams()
{
	OP_DELETE(ref_info);
	OP_DELETEA(path);
	OP_DELETEA(pattern);
}

DOM_JILDevice::JILDeviceFilesFoundListenerAsync::JILDeviceFilesFoundListenerAsync(DOM_Runtime* runtime, ES_Object* this_object, ES_Object* es_callback, OpAutoPtr<SearchParams> params) :
	DOM_AsyncCallback(runtime, es_callback, this_object),
	m_results_array_protector(NULL, runtime),
	m_search_params(params),
	m_finder(NULL),
	m_runtime(runtime)
{
}

OP_STATUS
DOM_JILDevice::JILDeviceFilesFoundListenerAsync::Construct()
{
	m_finder = OP_NEW(JILFileFinder, ());
	RETURN_OOM_IF_NULL(m_finder);
	RETURN_OOM_IF_NULL(m_search_params->ref_info);
	ES_Object* results_array;
	RETURN_IF_ERROR(GetRuntime()->CreateNativeArrayObject(&results_array, 0));
	m_results_array_protector.Reset(results_array);
	RETURN_IF_ERROR(m_results_array_protector.Protect());

	return OpStatus::OK;
}

void
DOM_JILDevice::JILDeviceFilesFoundListenerAsync::Start()
{
	m_finder->Find(m_search_params->path, m_search_params->pattern, this, TRUE, m_search_params->ref_info);
}

void
DOM_JILDevice::JILDeviceFilesFoundListenerAsync::OnFilesFound(OpVector<OpString> *results)
{
	int array_idx = 0;
	for (unsigned int cnt = m_search_params->start_index; results && cnt < results->GetCount() && cnt <= m_search_params->end_index; ++cnt)
	{
		OP_ASSERT(results->Get(cnt));
		ES_Value val;
		DOM_JILFile* new_file_obj;
		OP_STATUS status = DOM_JILFile::Make(new_file_obj, m_runtime, results->Get(cnt)->CStr());
		if (OpStatus::IsError(status))
			continue; // Skip this one and try to proceed.

		DOM_Object::DOMSetObject(&val, new_file_obj);
		if (OpStatus::IsError(m_runtime->PutIndex(m_results_array_protector.Get(), array_idx, val)))
		{
			OnError(OpStatus::ERR_NO_MEMORY); // No point to continue. Notify a caller with all you got
			return;
		}

		++array_idx;
	}
	OnFinished();
}

void
DOM_JILDevice::JILDeviceFilesFoundListenerAsync::OnFinished()
{
	ES_Value argv[1];
	DOM_Object::DOMSetObject(&argv[0], m_results_array_protector.Get());
	OpStatus::Ignore(CallCallback(argv, sizeof(argv) / sizeof(argv[0])));
	OP_DELETE(this);
}

void
DOM_JILDevice::JILDeviceFilesFoundListenerAsync::OnError(OP_STATUS error)
{
	OnFinished(); // Notification with an empty (or not complete) result array.
}

/* static */ int
DOM_JILDevice::findFiles(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(this_, DOM_TYPE_JIL_DEVICE, DOM_JILDevice);
	DOM_CHECK_ARGUMENTS_JIL("onN");
	DOM_ARGUMENT_OBJECT(jil_file, 0, DOM_TYPE_JIL_FILE, DOM_JILFile);

	// We allow infinity but disallow nulls.
	if (argv[2].type != VALUE_NUMBER || op_isnan(argv[2].value.number))
		return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, origining_runtime, UNI_L("End index invalid"));

	if (!g_DOM_jilUtils->RuntimeHasFilesystemAccess(origining_runtime))
	{
		this_object->CreateJILException(DOM_Object::JIL_SECURITY_ERR, return_value, origining_runtime);
		return ES_EXCEPTION;
	}

	OpAutoPtr<JILDeviceFilesFoundListenerAsync::SearchParams> params(OP_NEW(JILDeviceFilesFoundListenerAsync::SearchParams, ()));
	if (!params.get())
		return ES_NO_MEMORY;

	if (argv[2].value.number < 0 || argv[1].value.number < 0)
		return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, origining_runtime, UNI_L("Negative index value"));

	params->start_index = static_cast<unsigned int>(argv[1].value.number);
	params->end_index = argv[2].value.number < UINT_MAX ? static_cast<unsigned int>(argv[2].value.number) : UINT_MAX;
	params->ref_info = OP_NEW(OpFileInfo, ());

	if (!params->ref_info)
		return ES_NO_MEMORY;

	params->path = UniSetNewStr(jil_file->GetFilePath());
	params->pattern = UniSetNewStr(jil_file->GetFileName());
	if ((!params->path && jil_file->GetFilePath()) || (!params->pattern && jil_file->GetFileName()))
		return ES_NO_MEMORY;

	params->ref_info->creation_time = jil_file->GetCreationDate();
	params->ref_info->last_modified = jil_file->GetLastModifyDate();
	params->ref_info->length = jil_file->GetFileLength();
	params->ref_info->mode = (jil_file->IsDirectory() == YES) ? OpFileInfo::DIRECTORY : ((jil_file->IsDirectory() == NO) ? OpFileInfo::FILE : OpFileInfo::NOT_REGULAR);

	JILDeviceFilesFoundListenerAsync* callback = OP_NEW(JILDeviceFilesFoundListenerAsync
														, (origining_runtime, this_object->GetNativeObject()
														, this_->m_on_files_found, params));
	OpAutoPtr<JILDeviceFilesFoundListenerAsync> ap_callback(callback);
	if (!callback)
		return ES_NO_MEMORY;

	CALL_FAILED_IF_ERROR_WITH_HANDLER(callback->Construct(), HandleFileOperationError);

	callback->Start();

	ap_callback.release();

	return ES_FAILED;
}

/* static */ int
DOM_JILDevice::getFileSystemRoots(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_JIL_DEVICE);

	if (!g_DOM_jilUtils->RuntimeHasFilesystemAccess(origining_runtime))
	{
		this_object->CreateJILException(DOM_Object::JIL_SECURITY_ERR, return_value, origining_runtime);
		return ES_EXCEPTION;
	}

	ES_Object* file_system_roots_array;
	CALL_FAILED_IF_ERROR_WITH_HANDLER(origining_runtime->CreateNativeArrayObject(&file_system_roots_array, 0), HandleFileOperationError);
	ES_Value val;

	OpAutoVector<OpString> virtual_dirs;
	CALL_FAILED_IF_ERROR_WITH_HANDLER(g_DAPI_jil_fs_mgr->GetAvailableVirtualDirs(&virtual_dirs), HandleFileOperationError);

	int virtual_dirs_count = virtual_dirs.GetCount();

	for (int cnt = 0; cnt < virtual_dirs_count; ++cnt)
	{
		OP_ASSERT(virtual_dirs.Get(cnt));
		DOM_Object::DOMSetString(&val, virtual_dirs.Get(cnt)->CStr());
		CALL_FAILED_IF_ERROR_WITH_HANDLER(origining_runtime->PutIndex(file_system_roots_array, cnt, val), HandleFileOperationError);
	}

	DOMSetObject(return_value, file_system_roots_array);

	return ES_VALUE;
}

#ifdef USE_OP_CLIPBOARD
void DOM_JILDevice::OnPaste(OpClipboard* clipboard)
{
	OP_ASSERT(clipboard->HasText());
	clipboard->GetText(m_clipboard_string);
}

void DOM_JILDevice::OnCopy(OpClipboard* clipboard)
{
	OP_ASSERT(m_clipboard_string.CStr());
	clipboard->PlaceText(m_clipboard_string.CStr());
}
#endif // USE_OP_CLIPBOARD

/* static */ int
DOM_JILDevice::getFileSystemSize(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_JIL_DEVICE);
	DOM_CHECK_ARGUMENTS_JIL("s");

	if (!g_DOM_jilUtils->RuntimeHasFilesystemAccess(origining_runtime))
	{
		this_object->CreateJILException(DOM_Object::JIL_SECURITY_ERR, return_value, origining_runtime);
		return ES_EXCEPTION;
	}

	const uni_char* jil_filesystem = argv[0].value.string;

	UINT64 size = 0;
	CALL_FAILED_IF_ERROR_WITH_HANDLER(g_DAPI_jil_fs_mgr->GetFileSystemSize(jil_filesystem, &size), HandleFileOperationError);

	DOMSetNumber(return_value, static_cast<double>(size));

	return ES_VALUE;
}

/* virtual */ ES_GetState
DOM_JILDevice::InternalGetName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value)
{
	switch (property_atom)
	{
	case OP_ATOM_onFilesFound:
		DOMSetObject(value, m_on_files_found);

		return GET_SUCCESS;
#ifdef USE_OP_CLIPBOARD
	case OP_ATOM_clipboardString:
		if (value)
		{
			if (g_clipboard_manager->HasText())
			{
				m_clipboard_string.Empty();
				// Fills m_clipboard_string with clipboard's content.
				GET_FAILED_IF_ERROR(g_clipboard_manager->Paste(this));
				if (!m_clipboard_string.DataPtr())
					return GET_NO_MEMORY;
				TempBuffer* buf = GetEmptyTempBuf();
				GET_FAILED_IF_ERROR(buf->Append(m_clipboard_string));
				DOMSetString(value, buf);
			}
			else
				DOMSetString(value); // sets an empty string
		}
		return GET_SUCCESS;
#endif // USE_OP_CLIPBOARD
	}
	return DOM_JILObject::InternalGetName(property_atom, value, origining_runtime, restart_value);
}

/* virtual */ ES_PutState
DOM_JILDevice::InternalPutName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value)
{
	switch (property_atom)
	{
	case OP_ATOM_onFilesFound:
		switch (value->type)
		{
		case VALUE_OBJECT:
			m_on_files_found = value->value.object;
			return PUT_SUCCESS;
		case VALUE_NULL:
			m_on_files_found = NULL;
			return PUT_SUCCESS;
		default:
			return DOM_PUTNAME_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		}
#ifdef USE_OP_CLIPBOARD
	case OP_ATOM_clipboardString:
		switch (value->type)
		{
		case VALUE_UNDEFINED:
		case VALUE_NULL:
			// not specified what should happen here so we just ignore it
			return PUT_SUCCESS;
		case VALUE_STRING:
			PUT_FAILED_IF_ERROR(m_clipboard_string.Set(value->value.string));
			// Puts m_clipboard_string's content in the clipboard.
			PUT_FAILED_IF_ERROR(g_clipboard_manager->Copy(this));
			return PUT_SUCCESS;
		default:
			return PUT_NEEDS_STRING;
		}
#endif // USE_OP_CLIPBOARD
	}
	return DOM_JILObject::InternalPutName(property_atom, value, origining_runtime, restart_value);;
}

/* virtual */ void
DOM_JILDevice::GCTrace()
{
	DOM_JILObject::GCTrace();
	GCMark(m_on_files_found);
}

/* static */ int
DOM_JILDevice::HandleApplicationError(OP_STATUS error, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	switch (error)
	{
	case OpStatus::ERR_NO_SUCH_RESOURCE:
		return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, origining_runtime, UNI_L("Unknown application"));
	default:
		return HandleJILError(error, return_value, origining_runtime);
	}
}

/* static */ int
DOM_JILDevice::HandleFileOperationError(OP_STATUS error, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	switch (error)
	{
	case OpStatus::ERR_NO_DISK:
		return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, origining_runtime, UNI_L("No disk space left"));
	case OpStatus::ERR_NO_ACCESS:
		return CallException(DOM_Object::JIL_SECURITY_ERR, return_value, origining_runtime, UNI_L("Insufficient file access rights"));
	default:
		return HandleJILError(error, return_value, origining_runtime);
	}
}

/* static */ int
DOM_JILDevice::HandleDeleteFileOperationError(OP_STATUS error, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	switch (error)
	{
	case OpStatus::ERR:
	case OpStatus::ERR_NO_ACCESS:
		return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, origining_runtime, UNI_L("Could not delete a file"));
	default:
		return HandleJILError(error, return_value, origining_runtime);
	}
}

/* static */ int
DOM_JILDevice::setRingtone(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_JIL_DEVICE);
	DOM_CHECK_OR_RESTORE_ARGUMENTS_JIL("ss");

	const uni_char* ringtone_file_path = argv[0].value.string;
	const uni_char* addressbook_item_id = argv[1].value.string;
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
	 function(g_device_api_system_resource_setter, &SystemResourceSetter::SetSystemResource, ringtone_file_path, SystemResourceSetter::CONTACT_RINGTONE, addressbook_item_id, callback, origining_runtime->GetFramesDocument());
	DOM_SUSPENDING_CALL(call, function, JILUtils::SetSystemResourceCallbackSuspendingImpl, callback);
	OP_ASSERT(callback);

	CALL_FAILED_IF_ERROR_WITH_HANDLER(callback->GetErrorCode(), DOM_JILConfig::HandleSetSystemResourceError);
	return ES_FAILED;
}

/* static */ int
DOM_JILDevice::vibrate(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_JIL_DEVICE);
	DOM_CHECK_ARGUMENTS_JIL("n");

	double duration_seconds = argv[0].value.number;
	if (duration_seconds < 0.0)
		return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, origining_runtime, UNI_L("durationSeconds must be positive."));

	CALL_FAILED_IF_ERROR_WITH_HANDLER(g_op_system_info->Vibrate(static_cast<UINT32>(duration_seconds) * 1000), DOM_JILObject::HandleJILError);
	return ES_FAILED;
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_JILDevice)
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILDevice,    DOM_JILDevice::getAvailableApplications, "getAvailableApplications", "",     "Device.getAvailableApplications")
	DOM_FUNCTIONS_WITH_SECURITY_RULE_A1(DOM_JILDevice, DOM_JILDevice::getDirectoryFileNames,    "getDirectoryFileNames",    "s-",   "Device.getDirectoryFileNames", 0)
	DOM_FUNCTIONS_WITH_SECURITY_RULE_A1(DOM_JILDevice, DOM_JILDevice::getFile,                  "getFile",                  "s-",   "Device.getFile", 0)
	DOM_FUNCTIONS_WITH_SECURITY_RULE_A2(DOM_JILDevice, DOM_JILDevice::launchApplication,        "launchApplication",        "s|s-", "Device.launchApplication", 0, 1)
	DOM_FUNCTIONS_WITH_SECURITY_RULE_A2(DOM_JILDevice, DOM_JILDevice::copyFile,                 "copyFile",                 "ss-",  "Device.copyFile", 0, 1)
	DOM_FUNCTIONS_WITH_SECURITY_RULE_A1(DOM_JILDevice, DOM_JILDevice::deleteFile,               "deleteFile",               "s-",   "Device.deleteFile", 0)
	DOM_FUNCTIONS_WITH_SECURITY_RULE_A2(DOM_JILDevice, DOM_JILDevice::moveFile,                 "moveFile",                 "ss-",  "Device.moveFile", 0, 1)
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILDevice,    DOM_JILDevice::findFiles,                "findFiles",                "onN-", "Device.findFiles")
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILDevice,    DOM_JILDevice::getFileSystemRoots,       "getFileSystemRoots",       "",     "Device.getFileSystemRoots")
	DOM_FUNCTIONS_WITH_SECURITY_RULE_A1(DOM_JILDevice, DOM_JILDevice::getFileSystemSize,        "getFileSystemSize",        "s-",   "Device.getFileSystemSize", 0)
	DOM_FUNCTIONS_WITH_SECURITY_RULE_A1(DOM_JILDevice, DOM_JILDevice::setRingtone,              "setRingtone",              "ss-",  "Device.setRingtone", 0)
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILDevice,    DOM_JILDevice::setRingtone,              "setRingtone",              "ss-",  "Device.setRingtone")
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILDevice,    DOM_JILDevice::vibrate,                  "vibrate",                  "n-",   "Device.vibrate")
DOM_FUNCTIONS_END(DOM_JILDevice)

#endif // DOM_JIL_API_SUPPORT
