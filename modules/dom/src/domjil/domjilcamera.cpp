/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef JIL_CAMERA_SUPPORT
#include "modules/dom/src/domjil/domjilcamera.h"
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/ecmascript_utils/esasyncif.h"
#include "modules/dom/src/domjil/utils/jilutils.h"
#include "modules/dom/src/domcore/element.h"
#include "modules/device_api/jil/JILFSMgr.h"
#include "modules/media/camerapreview.h"
#include "modules/media/camerarecorder.h"

DOM_JILCamera::DOM_JILCamera()
	: m_on_camera_captured(NULL)
	, m_element(NULL)
{
}

DOM_JILCamera::~DOM_JILCamera()
{
	CameraRecorder* camera_recorder;
	if (OpStatus::IsSuccess(g_media_module.GetCameraRecorder(camera_recorder)))
		camera_recorder->AbortCapture(this);
}

/* static */
OP_STATUS DOM_JILCamera::Make(DOM_JILCamera*& new_jil_camera, DOM_Runtime* runtime)
{
	OP_ASSERT(runtime);

	new_jil_camera = OP_NEW(DOM_JILCamera, ());
	RETURN_IF_ERROR(DOMSetObjectRuntime(new_jil_camera, runtime, runtime->GetPrototype(DOM_Runtime::JIL_CAMERA_PROTOTYPE), "Camera"));
	return OpStatus::OK;
}

/*virtual */
void DOM_JILCamera::GCTrace()
{
	DOM_JILObject::GCTrace();
	GCMark(m_on_camera_captured);
	GCMark(m_element);
}

/* static */ int
DOM_JILCamera::setWindow(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(jilcamera, DOM_TYPE_JIL_CAMERA, DOM_JILCamera);

	DOM_CHECK_ARGUMENTS_JIL("O");
	DOM_ARGUMENT_JIL_OBJECT(dom_element, 0, DOM_TYPE_ELEMENT, DOM_Element);

	if (dom_element)
	{
		OP_ASSERT(dom_element->GetThisElement());

		if (!dom_element->GetThisElement()->IsMatchingType(HE_OBJECT, NS_HTML))
			return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, origining_runtime, UNI_L("Camera preview requires an <object> element."));
	}

	CALL_FAILED_IF_ERROR_WITH_HANDLER(jilcamera->SetWindowInternal(dom_element), HandleCameraError);
	return OpStatus::OK;
}

/* static */ int
DOM_JILCamera::captureImage(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(jilcamera, DOM_TYPE_JIL_CAMERA, DOM_JILCamera);
	DOM_CHECK_ARGUMENTS_JIL("Sb");

	const uni_char* suggested_filename;
	if (argv[0].type == VALUE_STRING)
		suggested_filename = argv[0].value.string;
	else
		suggested_filename = UNI_L("");

	if (!g_DOM_jilUtils->RuntimeHasFilesystemAccess(origining_runtime))
		return CallException(DOM_Object::JIL_SECURITY_ERR, return_value, origining_runtime, UNI_L("File access not permitted"));

	OpString system_filename;
	if (suggested_filename[0] != 0)
		CALL_FAILED_IF_ERROR_WITH_HANDLER(g_DAPI_jil_fs_mgr->JILToSystemPath(suggested_filename, system_filename), HandleCameraError);

	CameraRecorder* camera_recorder;
	CALL_FAILED_IF_ERROR_WITH_HANDLER(g_media_module.GetCameraRecorder(camera_recorder), HandleCameraError);
	CALL_FAILED_IF_ERROR_WITH_HANDLER(camera_recorder->CaptureImage(&system_filename, argv[1].value.boolean, jilcamera), HandleCameraError);

	OpString jil_filename;
	CALL_FAILED_IF_ERROR_WITH_HANDLER(g_DAPI_jil_fs_mgr->SystemToJILPath(system_filename, jil_filename), HandleCameraError);

	TempBuffer* buffer = GetEmptyTempBuf();
	OP_STATUS status = buffer->Append(jil_filename.CStr());
	CALL_FAILED_IF_ERROR_WITH_HANDLER(status, HandleCameraError);

	DOMSetString(return_value, buffer);
	return ES_VALUE;
}

/* static */ int
DOM_JILCamera::startVideoCapture(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(jilcamera, DOM_TYPE_JIL_CAMERA, DOM_JILCamera);
	DOM_CHECK_ARGUMENTS_JIL("Sbnb");

	UINT32 requested_capture_time = 0;
	double requested_time = argv[2].value.number * 1000;

	if (requested_time < 0)
		return HandleCameraError(OpStatus::ERR_OUT_OF_RANGE, return_value, origining_runtime);

	if (requested_time == 0)
		return ES_FAILED; // This is completely useless, but in a spec. TODO - try to convince JIL to change it.

	if (requested_time < 0xFFFFFFFF)
		requested_capture_time = static_cast<UINT32>(requested_time);
	else
		requested_capture_time = 0xFFFFFFFF;

	CameraPreview *camera_element = jilcamera->GetJILCameraPreview();

	if (!g_DOM_jilUtils->RuntimeHasFilesystemAccess(origining_runtime))
		return CallException(DOM_Object::JIL_SECURITY_ERR, return_value, origining_runtime, UNI_L("File access not permitted"));

	const uni_char* suggested_filename;
	if (argv[0].type == VALUE_STRING)
		suggested_filename = argv[0].value.string;
	else
		suggested_filename = UNI_L("");

	CameraRecorder* camera_recorder;
	CALL_FAILED_IF_ERROR_WITH_HANDLER(g_media_module.GetCameraRecorder(camera_recorder), HandleCameraError);

	BOOL lowRes = argv[1].value.boolean;
	BOOL showControls = argv[3].value.boolean;
	OpString system_filename;
	CALL_FAILED_IF_ERROR_WITH_HANDLER(g_DAPI_jil_fs_mgr->JILToSystemPath(suggested_filename, system_filename), HandleCameraError);
	if (!showControls) // just start video ...
		CALL_FAILED_IF_ERROR_WITH_HANDLER(camera_recorder->CaptureVideo(&system_filename, requested_capture_time, lowRes, jilcamera), HandleCameraError);
	else if (camera_element) // ... or just show controls - Don't ask me whether it makes sense - I didn't write that spec
	{
		CameraPreview::RecordingParams params;
		CALL_FAILED_IF_ERROR_WITH_HANDLER(params.filename.Set(suggested_filename), HandleCameraError);
		params.listener = jilcamera;
		params.low_res = lowRes;
		params.max_duration_milliseconds = requested_capture_time;
		camera_element->EnableControls(TRUE, params);
	}
	OpString jil_filename;
	OP_STATUS status = g_DAPI_jil_fs_mgr->SystemToJILPath(system_filename.CStr(), jil_filename);

	if (OpStatus::IsError(status))
	{
		// Try to stop the video capture.
		OpStatus::Ignore(camera_recorder->StopVideoCapture());
		CALL_FAILED_IF_ERROR_WITH_HANDLER(status, HandleCameraError);
	}

	TempBuffer* buffer = GetEmptyTempBuf();
	status = buffer->Append(jil_filename.CStr());
	if (OpStatus::IsError(status))
	{
		// Try to stop the video capture.
		OpStatus::Ignore(camera_recorder->StopVideoCapture());
		CALL_FAILED_IF_ERROR_WITH_HANDLER(status, HandleCameraError);
	}

	DOMSetString(return_value, buffer);

	return ES_VALUE;
}

/* static */ int
DOM_JILCamera::stopVideoCapture(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_JIL_CAMERA);
	CameraRecorder* camera_recorder;
	CALL_FAILED_IF_ERROR_WITH_HANDLER(g_media_module.GetCameraRecorder(camera_recorder), HandleCameraError);
	OpStatus::Ignore(camera_recorder->StopVideoCapture());
	return ES_FAILED;
}

/* virtual */ void
DOM_JILCamera::OnCameraCaptured(OpCamera* camera, const uni_char* file_name)
{
	OnCameraCaptureFinished(OpStatus::OK, file_name);
}

/* virtual */ void
DOM_JILCamera::OnCameraCaptureFailed(OpCamera* camera, OP_CAMERA_STATUS error)
{
	OnCameraCaptureFinished(error, NULL);
}

/* virtual */ void
DOM_JILCamera::OnCameraCaptureFinished(OP_STATUS result, const uni_char* file_name)
{
	CameraPreview *camera_element = GetJILCameraPreview();
	if (camera_element)
	{
		CameraPreview::RecordingParams dummy;
		camera_element->EnableControls(FALSE, dummy);
	}

	if (m_on_camera_captured)
	{
		ES_AsyncInterface* async_iface = GetRuntime()->GetEnvironment()->GetAsyncInterface();
		ES_Value argv[1];
		OpString jil_filename;

		if (OpStatus::IsSuccess(result) && file_name)
		{
			g_DAPI_jil_fs_mgr->SystemToJILPath(file_name, jil_filename);
			DOMSetString(&(argv[0]), jil_filename.CStr());
		}
		else
			DOMSetNull(&(argv[0]));

		async_iface->CallFunction(m_on_camera_captured, GetNativeObject(), sizeof(argv) / sizeof(argv[0]), argv, NULL, NULL);
	}
}

/* virtual */ ES_GetState
DOM_JILCamera::InternalGetName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value)
{
	switch (property_atom)
	{
		case OP_ATOM_onCameraCaptured:
			DOMSetObject(value, m_on_camera_captured);
			return GET_SUCCESS;
	}

	return GET_FAILED;
}

/* virtual */ ES_PutState
DOM_JILCamera::InternalPutName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value)
{
	switch (property_atom)
	{
		case OP_ATOM_onCameraCaptured:
			switch (value->type)
			{
			case VALUE_OBJECT:
				m_on_camera_captured = value->value.object;
				return PUT_SUCCESS;
			case VALUE_NULL:
				m_on_camera_captured = NULL;
				return PUT_SUCCESS;
			}
			return DOM_PUTNAME_DOMEXCEPTION(TYPE_MISMATCH_ERR);
	}
	return PUT_FAILED;
}

OP_CAMERA_STATUS
DOM_JILCamera::SetWindowInternal(DOM_Node *dom_node)
{
	HTML_Element *element = m_element ? m_element->GetThisElement() : NULL;
	HTML_Element *new_element = dom_node ? dom_node->GetThisElement() : NULL;

	// Reset media attribute and release any existing media elements.
	if (element)
	{
		element->SetMedia(Markup::MEDA_COMPLEX_CAMERA_PREVIEW, NULL, FALSE);
		if (m_element->GetFramesDocument())
			element->MarkExtraDirty(m_element->GetFramesDocument());
	}

	if (new_element)
	{
		CameraPreview* camera_preview = OP_NEW(CameraPreview, (new_element));
		if (!camera_preview)
			return OpStatus::ERR_NO_MEMORY;

		if (OpStatus::IsError(camera_preview->EnablePreview()))
		{
			OP_DELETE(camera_preview);
			return OpCameraStatus::ERR_NOT_AVAILABLE;
		}

		// set HTML_Element attributes - from now on 'camera_element' is owned by 'element'
		if (!new_element->SetMedia(Markup::MEDA_COMPLEX_CAMERA_PREVIEW, camera_preview, TRUE))
		{
			OP_DELETE(camera_preview);
			return OpStatus::ERR_NO_MEMORY;
		}

		new_element->MarkExtraDirty(GetEnvironment()->GetFramesDocument());
	}

	m_element = dom_node;
	return OpStatus::OK;
}


/* static */ int
DOM_JILCamera::HandleCameraError(OP_STATUS error, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	switch (error)
	{
	case OpCameraStatus::ERR_NO_ACCESS:
		return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, origining_runtime, UNI_L("Access to file not permitted"));
	case OpCameraStatus::ERR_OPERATION_ABORTED:
		return CallException(DOM_Object::JIL_UNKNOWN_ERR, return_value, origining_runtime, UNI_L("Operation aborted"));
	case OpCameraStatus::ERR_FILE_EXISTS:
		return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, origining_runtime, UNI_L("File already exists"));
	case OpCameraStatus::ERR_NOT_AVAILABLE:
		return CallException(DOM_Object::JIL_UNKNOWN_ERR, return_value, origining_runtime, UNI_L("Camera not available"));
	case OpStatus::ERR_NULL_POINTER:
		return CallException(DOM_Object::JIL_UNKNOWN_ERR, return_value, origining_runtime, UNI_L("Camera preview window not set!"));

	default:
		return HandleJILError(error, return_value, origining_runtime);
	}
}

CameraPreview*
DOM_JILCamera::GetJILCameraPreview()
{
	if (!m_element)
		return NULL;
	HTML_Element* element = m_element->GetThisElement();
	if (element)
		return static_cast<CameraPreview *>(element->GetSpecialAttr(Markup::MEDA_COMPLEX_CAMERA_PREVIEW, ITEM_TYPE_COMPLEX, NULL, SpecialNs::NS_MEDIA));

	return NULL;
}



#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_JILCamera)
	DOM_FUNCTIONS_WITH_SECURITY_RULE_A1(DOM_JILCamera, DOM_JILCamera::captureImage,			"captureImage",		"Sb-",		"Camera.captureImage", 0)
	DOM_FUNCTIONS_WITH_SECURITY_RULE_A1(DOM_JILCamera, DOM_JILCamera::startVideoCapture,	"startVideoCapture","Sbnb-",	"Camera.startVideoCapture", 0)
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILCamera, DOM_JILCamera::stopVideoCapture,		"stopVideoCapture",	"-",		"Camera.stopVideoCapture")
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILCamera, DOM_JILCamera::setWindow,				"setWindow",		"-",		"Camera.setWindow")
DOM_FUNCTIONS_END(DOM_JILCamera)

#endif // JIL_CAMERA_SUPPORT
