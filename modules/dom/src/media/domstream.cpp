/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include <core/pch.h>

#ifdef DOM_STREAM_API_SUPPORT

#include "modules/dom/src/media/domstream.h"

#include "modules/dom/src/domcore/node.h"
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domevents/domevent.h"

#include "modules/media/camerarecorder.h"

/* static */ OP_STATUS
DOM_LocalMediaStream::Make(DOM_LocalMediaStream *&new_stream_object, DOM_Runtime *runtime)
{
	OP_ASSERT(runtime);

	new_stream_object = OP_NEW(DOM_LocalMediaStream, ());
	RETURN_IF_ERROR(DOMSetObjectRuntime(new_stream_object, runtime, runtime->GetPrototype(DOM_Runtime::LOCALMEDIASTREAM_PROTOTYPE), "LocalMediaStream"));
	return new_stream_object->Initialize();
}

DOM_LocalMediaStream::~DOM_LocalMediaStream()
{
	if (m_stream_controller)
	{
		m_stream_controller->OnStreamObjectDestroyed();
		m_stream_controller->DecReferenceCount();
	}
}

OP_STATUS
DOM_LocalMediaStream::Initialize()
{
	RETURN_IF_ERROR(DOM_StreamController::Make(m_stream_controller, this));
	OP_ASSERT(m_stream_controller);
	m_stream_controller->IncReferenceCount();
	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_LocalMediaStream::GetName(const uni_char *property_name, int property_code, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_name[0] == 'o' && property_name[1] == 'n')
		return GetEventProperty(property_name, value, static_cast<DOM_Runtime *>(origining_runtime));
	else
		return DOM_Object::GetName(property_name, property_code, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_LocalMediaStream::PutName(const uni_char *property_name, int property_code, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_name[0] == 'o' && property_name[1] == 'n')
		return PutEventProperty(property_name, value, static_cast<DOM_Runtime *>(origining_runtime));
	else
		return DOM_Object::PutName(property_name, property_code, value, origining_runtime);
}

/* virtual */ int
DOM_LocalMediaStream::createObjectURL(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_LOCALMEDIASTREAM);
	DOMSetString(return_value, UNI_L("about:streamurl"));
	return ES_VALUE;
}

/* virtual */ void
DOM_LocalMediaStream::GCTrace()
{
	GCMark(FetchEventTarget());
}

void
DOM_LocalMediaStream::Stop()
{
	m_is_stream_live = FALSE;

	DOM_Event *onended = OP_NEW(DOM_Event, ());
	RETURN_VOID_IF_ERROR(DOMSetObjectRuntime(onended, GetRuntime(),
		GetRuntime()->GetPrototype(DOM_Runtime::EVENT_PROTOTYPE), "Event"));
	onended->InitEvent(MEDIAENDED, this); // This has nothing to do with HTMLMediaElement.onended
	OpStatus::Ignore(GetEnvironment()->SendEvent(onended));
}

OP_STATUS
DOM_LocalMediaStream::AssociateWithMediaElement(MediaElement *element)
{
	OP_ASSERT(m_is_stream_live);
	element->SetStreamResource(m_stream_controller);
	return OpStatus::OK;
}

DOM_StreamController::~DOM_StreamController()
{
	OP_ASSERT(m_dom_stream == NULL);
	OP_ASSERT(!OpListenable<MediaStreamListener>::HasListeners());
	OP_ASSERT(!OpListenable<MediaStreamListener>::IsLocked());
	DetachListeners();
}

/* static */ OP_STATUS
DOM_StreamController::Make(DOM_StreamController*& new_obj, DOM_LocalMediaStream* dom_stream)
{
	CameraRecorder* camera_recorder;
	RETURN_IF_ERROR(g_media_module.GetCameraRecorder(camera_recorder));

	OpAutoPtr<DOM_StreamController> new_obj_ap(OP_NEW(DOM_StreamController, (dom_stream->GetRuntime()->GetOriginURL())));
	RETURN_OOM_IF_NULL(new_obj_ap.get());
	new_obj_ap->m_runtime = dom_stream->GetRuntime();
	RETURN_IF_ERROR(new_obj_ap->AttachUserConsentListener());
	new_obj_ap->m_dom_stream = dom_stream;
	RETURN_IF_ERROR(camera_recorder->AttachCameraRemovedListener(new_obj_ap.get()));
	new_obj = new_obj_ap.release();
	return OpStatus::OK;
}

/* virtual */ const URL &
DOM_StreamController::GetOriginURL()
{
	return m_origin_url;
}

/* static */ OP_STATUS
DOM_StreamController::ConvertCameraStatus(OP_CAMERA_STATUS camera_status)
{
	switch(camera_status)
	{
	case OpCameraStatus::ERR_NOT_AVAILABLE:
	case OpCameraStatus::ERR_DISABLED:
		return OpStatus::ERR_NO_SUCH_RESOURCE;
	case OpCameraStatus::ERR_OPERATION_ABORTED:
	case OpCameraStatus::ERR_FILE_EXISTS:
	case OpCameraStatus::ERR_RESOLUTION_NOT_SUPPORTED:
		OP_ASSERT(!"Unexpected error status");
		return OpStatus::ERR;
	default:
		// regular OpStatus values
		return camera_status;
	}
}

/* virtual */ OP_STATUS
DOM_StreamController::AttachStreamListener(MediaStreamListener* listener)
{
	CameraRecorder* camera_recorder;
	RETURN_IF_ERROR(g_media_module.GetCameraRecorder(camera_recorder));
	RETURN_IF_ERROR(OpListenable<MediaStreamListener>::AttachListener(listener));

	if (OpListenable<MediaStreamListener>::GetListeners().GetCount() == 1)
	{
		OP_STATUS status = AttachCamera();
		if (OpStatus::IsError(status))
		{
			OpStatus::Ignore(OpListenable<MediaStreamListener>::DetachListener(listener));
			return status;
		}
	}
	return OpStatus::OK;
}

/* virtual */ void
DOM_StreamController::DetachStreamListener(MediaStreamListener* listener)
{
	RETURN_VOID_IF_ERROR(OpListenable<MediaStreamListener>::DetachListener(listener));

	if (!OpListenable<MediaStreamListener>::HasListeners())
		DetachCamera();
}

/* virtual */ OP_STATUS
DOM_StreamController::GetCurrentFrame(OpBitmap*& frame)
{
	if (m_blocked)
		return OpStatus::ERR_NO_ACCESS;

	CameraRecorder* camera_recorder;
	RETURN_IF_ERROR(g_media_module.GetCameraRecorder(camera_recorder));
	return camera_recorder->GetCurrentPreviewFrame(frame);
}

/* virtual */ OP_STATUS
DOM_StreamController::GetFrameDimensions(unsigned& width, unsigned& height)
{
	if (m_blocked)
		return OpStatus::ERR_NO_ACCESS;

	CameraRecorder* camera_recorder;
	RETURN_IF_ERROR(g_media_module.GetCameraRecorder(camera_recorder));
	return camera_recorder->GetPreviewDimensions(width, height);
}

/* virtual */ void
DOM_StreamController::OnCameraUpdateFrame(OpCamera* camera)
{
	if (m_blocked)
		return;

	NotifyUpdateFrame(this);
}

/* virtual */ void
DOM_StreamController::OnCameraFrameResize(OpCamera* camera)
{
	if (m_blocked)
		return;

	NotifyFrameResize(this);
}

/* virtual */ void
DOM_StreamController::OnCameraRemoved()
{
	StreamFinished();
}

/* virtual */ OP_STATUS
DOM_StreamController::AttachCamera()
{
	CameraRecorder* camera_recorder;
	RETURN_IF_ERROR(g_media_module.GetCameraRecorder(camera_recorder));
	OP_CAMERA_STATUS status = camera_recorder->AttachPreviewListener(this);
	if (OpStatus::IsError(status))
		return ConvertCameraStatus(status);
	OP_ASSERT(m_runtime);
	FramesDocument* doc = m_runtime->GetFramesDocument();
	OP_ASSERT(doc);
	doc->IncCameraUseCount();
	return OpStatus::OK;
}

/* virtual */ void
DOM_StreamController::DetachCamera()
{
	OP_ASSERT(!OpListenable<MediaStreamListener>::HasListeners());
	CameraRecorder* camera_recorder;
	RETURN_VOID_IF_ERROR(g_media_module.GetCameraRecorder(camera_recorder));
	camera_recorder->DetachPreviewListener(this);
	OP_ASSERT(m_runtime);
	FramesDocument* doc = m_runtime->GetFramesDocument();
	if (doc)
		doc->DecCameraUseCount();
}

OP_STATUS
DOM_StreamController::AttachUserConsentListener()
{
	OP_ASSERT(!m_user_consent_listener_attached);
	RETURN_IF_ERROR(g_secman_instance->RegisterUserConsentListener(OpSecurityManager::PERMISSION_TYPE_CAMERA, m_runtime, this));
	m_user_consent_listener_attached = TRUE;
	return OpStatus::OK;
}

void DOM_StreamController::DetachUserConsentListener()
{
	if (m_runtime && m_user_consent_listener_attached)
	{
		g_secman_instance->UnregisterUserConsentListener(OpSecurityManager::PERMISSION_TYPE_CAMERA, m_runtime, this);
		m_user_consent_listener_attached = FALSE;
	}
}

/* virtual */ void
DOM_StreamController::OnUserConsentRevoked()
{
	StreamFinished();
}

void
DOM_StreamController::StreamFinished()
{
	NotifyStreamFinished(this);
	m_blocked = TRUE;

	DetachListeners();

	OP_ASSERT(!OpListenable<MediaStreamListener>::IsLocked());
	if (OpListenable<MediaStreamListener>::m_listeners.GetCount())
	{
		OpListenable<MediaStreamListener>::m_listeners.Clear();
		DetachCamera();
	}
	if (m_dom_stream)
		m_dom_stream->Stop();
}

/* virtual */ void
DOM_StreamController::OnBeforeRuntimeDestroyed(DOM_Runtime *runtime)
{
	OP_ASSERT(runtime == m_runtime);
	DetachListeners();
}

void
DOM_StreamController::DetachListeners()
{
	DetachUserConsentListener();

	CameraRecorder* camera_recorder;
	if (OpStatus::IsSuccess(g_media_module.GetCameraRecorder(camera_recorder)))
		camera_recorder->DetachCameraRemovedListener(this);
}

void
DOM_StreamController::OnStreamObjectDestroyed()
{
	m_dom_stream = NULL;
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_LocalMediaStream)
	DOM_FUNCTIONS_FUNCTION(DOM_LocalMediaStream, DOM_LocalMediaStream::createObjectURL, "createObjectURL", NULL)
DOM_FUNCTIONS_END(DOM_Stream)

DOM_FUNCTIONS_WITH_DATA_START(DOM_LocalMediaStream)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_LocalMediaStream, DOM_Node::accessEventListener, 0, "addEventListener", "s-b-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_LocalMediaStream, DOM_Node::accessEventListener, 1, "removeEventListener", "s-b-")
DOM_FUNCTIONS_WITH_DATA_END(DOM_LocalMediaStream)

#endif // DOM_STREAM_API_SUPPORT
