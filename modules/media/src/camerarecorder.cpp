/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2010-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef MEDIA_CAMERA_SUPPORT

#include "modules/media/camerarecorder.h"

CameraRecorder::CameraRecorder()
	: m_camera(NULL)
	, m_enabled(FALSE)
#ifdef MEDIA_CAMERA_CAPTURE_SUPPORT
	, m_current_capture_listener(NULL)
	, m_recording_video(FALSE)
	, m_recording_video_paused(FALSE)
	, m_capturing_image(FALSE)
	, m_camera_supports_pause(FALSE)
#endif // MEDIA_CAMERA_CAPTURE_SUPPORT
{
}

CameraRecorder::~CameraRecorder()
{
	if (GetCamera())
		GetCamera()->Disable();
	g_op_camera_manager->SetCameraDeviceListener(NULL);
	OP_ASSERT(OpListenable<OpCameraPreviewListener>::m_listeners.GetCount() == 0);
	OP_ASSERT(OpListenable<CameraRemovedListener>::m_listeners.GetCount() == 0);
}

/* static */ OP_CAMERA_STATUS
CameraRecorder::Create(CameraRecorder*& new_camera_recorder)
{
	OpAutoPtr<CameraRecorder> new_recorder(OP_NEW(CameraRecorder, ()));
	RETURN_OOM_IF_NULL(new_recorder.get());
	RETURN_IF_ERROR(new_recorder->Init());
	new_camera_recorder = new_recorder.release();
	return OpStatus::OK;
}

OP_STATUS
CameraRecorder::Init()
{
	OpVector<OpCamera> cameras;
	RETURN_IF_ERROR(g_op_camera_manager->GetCameraDevices(&cameras));

#ifdef MEDIA_CAMERA_CAPTURE_SUPPORT
	// Choose the camera with highest photo resolution
	// Eventually we will support multiple cameras and allow the user to
	// choose one so this algorithm will be removed.
	OpCamera* best_camera = NULL;
	int best_width = 0;

	UINT32 count = cameras.GetCount();
	for (UINT32 i = 0; i < count; i++)
	{
		OpCamera* camera = cameras.Get(i);
		OpAutoVector<OpCamera::Resolution> resolutions;
		RETURN_IF_ERROR(camera->GetSupportedImageResolutions(&resolutions));

		int largest_width = resolutions.Get(resolutions.GetCount() - 1)->x;
		if (best_width < largest_width)
		{
			best_camera = camera;
			best_width = largest_width;
		}
	}

	// best_camera may be NULL here, but it's ok.
	OnCameraAttached(best_camera);
#else
	// Since there is only single camera support, just take the first one.
	// Eventually we will support multiple cameras and allow the user to
	// choose one.
	if (cameras.GetCount() > 0)
		OnCameraAttached(cameras.Get(0));
#endif // MEDIA_CAMERA_CAPTURE_SUPPORT
	g_op_camera_manager->SetCameraDeviceListener(this);
	return OpStatus::OK;
}

BOOL
CameraRecorder::IsCameraAvailable()
{
#ifdef MEDIA_CAMERA_CAPTURE_SUPPORT
	return HasCamera() && !m_recording_video && !m_capturing_image;
#else
	return HasCamera();
#endif // MEDIA_CAMERA_CAPTURE_SUPPORT
}

OP_CAMERA_STATUS
CameraRecorder::AttachPreviewListener(OpCameraPreviewListener* listener)
{
	RETURN_IF_ERROR(EnableCamera());
	OP_CAMERA_STATUS error = OpListenable<OpCameraPreviewListener>::AttachListener(listener);
	if (OpStatus::IsError(error))
		DisableCameraIfNotNeeded();
	return error;
}

void
CameraRecorder::DetachPreviewListener(OpCameraPreviewListener* listener)
{
	OpListenable<OpCameraPreviewListener>::DetachListener(listener);
	DisableCameraIfNotNeeded();
}

OP_CAMERA_STATUS CameraRecorder::EnableCamera()
{
	if (!HasCamera())
		return OpCameraStatus::ERR_NOT_AVAILABLE;

	OP_CAMERA_STATUS status = OpStatus::OK;
	if (!m_enabled)
	{
		status = GetCamera()->Enable(this);
		if (OpStatus::IsSuccess(status))
			m_enabled = TRUE;
	}
	return status;
}

void CameraRecorder::DisableCameraIfNotNeeded()
{
	OP_ASSERT(HasCamera());
	if (m_enabled && !IsPreviewNeeded()
#ifdef MEDIA_CAMERA_CAPTURE_SUPPORT
		&& !m_recording_video && !m_capturing_image
#endif // MEDIA_CAMERA_CAPTURE_SUPPORT
		)
	{
		GetCamera()->Disable();
		m_enabled = FALSE;
	}
}

#ifdef MEDIA_CAMERA_CAPTURE_SUPPORT

/** Capture image */
OP_CAMERA_STATUS CameraRecorder::CaptureImage(OpString* file_name, BOOL lowRes, OpCameraCaptureListener* listener)
{
	OP_ASSERT(listener);
	if (!IsCameraAvailable())
		return OpCameraStatus::ERR_NOT_AVAILABLE;

	OpCamera::CaptureParams params;

	if (lowRes)
	{
		OpAutoVector<OpCamera::Resolution> resolutions;
		RETURN_IF_ERROR(GetCamera()->GetSupportedImageResolutions(&resolutions));
		params.resolution = *resolutions.Get(0);
	}
	RETURN_IF_ERROR(EnableCamera());
	params.max_duration_miliseconds = 0;

	OP_CAMERA_STATUS error = GetCamera()->CaptureImage(file_name, this, params);
	if (OpStatus::IsError(error))
	{
		DisableCameraIfNotNeeded();
		return error;
	}
	m_current_capture_listener = listener;
	m_capturing_image = TRUE;

	return OpStatus::OK;
}

/** Start video capture */
OP_CAMERA_STATUS CameraRecorder::CaptureVideo(OpString* file_name, UINT32 requested_capture_time, BOOL low_res, OpCameraCaptureListener* listener)
{
	OP_ASSERT(listener);
	if (!IsCameraAvailable())
		return OpCameraStatus::ERR_NOT_AVAILABLE;

	OpCamera::CaptureParams params;

	if (low_res)
	{
		OpAutoVector<OpCamera::Resolution> resolutions;
		RETURN_IF_ERROR(GetCamera()->GetSupportedImageResolutions(&resolutions));
		params.resolution = *resolutions.Get(0);
	}

	RETURN_IF_ERROR(EnableCamera());
	params.max_duration_miliseconds = requested_capture_time;

	OP_CAMERA_STATUS error = GetCamera()->StartVideoCapture(file_name, this, params);
	if (OpStatus::IsError(error))
	{
		DisableCameraIfNotNeeded();
		return error;
	}
	m_current_capture_listener = listener;
	m_recording_video = TRUE;
	m_recording_video_paused = FALSE;

	return OpStatus::OK;
}

/** Pause or resume video capture */
OP_CAMERA_STATUS CameraRecorder::PauseVideoCapture(BOOL pause)
{
	if (!IsPauseSupported()) // this always returns FALSE now
		return OpStatus::ERR_NOT_SUPPORTED;

	m_recording_video_paused = pause;
	// PI doesn't support pausing yet.
	OP_ASSERT(!"Call OpCamera::Pause(m_recording_video_paused);");
	return OpStatus::OK;
}

/** Stop video capture */
OP_CAMERA_STATUS CameraRecorder::StopVideoCapture()
{
	if (m_recording_video)
		return GetCamera()->StopVideoCapture();
	else
		return OpStatus::OK;
}


void CameraRecorder::AbortCapture(OpCameraCaptureListener* listener)
{
	if (m_current_capture_listener == listener)
	{
		m_current_capture_listener = NULL;
		OpStatus::Ignore(StopVideoCapture());
	}
}

/** Resume video capture */
OP_CAMERA_STATUS CameraRecorder::ResumeVideoCapture()
{
	return OpStatus::ERR_NOT_SUPPORTED;
}

void CameraRecorder::FinishCaptureOperation(OpCamera* camera, OP_CAMERA_STATUS error, const uni_char* file_name)
{
	OP_ASSERT(camera == GetCamera());
	if (m_current_capture_listener)
	{
		if (OpStatus::IsError(error))
			m_current_capture_listener->OnCameraCaptureFailed(camera, error);
		else
			m_current_capture_listener->OnCameraCaptured(camera, file_name);
	}
	m_current_capture_listener = NULL;
	m_recording_video = FALSE;
	m_recording_video_paused = FALSE;
	m_capturing_image = FALSE;
	DisableCameraIfNotNeeded();
}

/** Camera capture success listener */
/* virtual */ void
CameraRecorder::OnCameraCaptured(OpCamera* camera, const uni_char* file_name)
{
	FinishCaptureOperation(camera, OpStatus::OK, file_name);
}

/** Camera capture failure listener */
/* virtual */ void
CameraRecorder::OnCameraCaptureFailed(OpCamera* camera, OP_CAMERA_STATUS error)
{
	FinishCaptureOperation(camera, error, NULL);
}

#endif // MEDIA_CAMERA_CAPTURE_SUPPORT

/** Camera frame update listener */
/* virtual */ void
CameraRecorder::OnCameraUpdateFrame(OpCamera* camera)
{
	NotifyCameraUpdateFrame(camera);
}

/* virtual */ void
CameraRecorder::OnCameraFrameResize(OpCamera* camera)
{
	NotifyCameraFrameResize(camera);
}

OP_CAMERA_STATUS
CameraRecorder::GetCurrentPreviewFrame(OpBitmap*& frame)
{
	if (GetCamera())
		return GetCamera()->GetCurrentPreviewFrame(&frame);
	else
		return OpCameraStatus::ERR_NOT_AVAILABLE;
}

OP_CAMERA_STATUS
CameraRecorder::GetPreviewDimensions(unsigned int& width, unsigned int& height)
{
	if (GetCamera())
	{
		OpCamera::Resolution dimensions;
		RETURN_IF_ERROR(GetCamera()->GetPreviewDimensions(&dimensions));
		width = dimensions.x;
		height = dimensions.y;
		return OpStatus::OK;
	}
	else
		return OpCameraStatus::ERR_NOT_AVAILABLE;
}

/* virtual */ void
CameraRecorder::OnCameraAttached(OpCamera* camera)
{
	if (!HasCamera())
		m_camera = camera;
}

/* virtual */ void
CameraRecorder::OnCameraDetached(OpCamera* camera)
{
	if (camera == m_camera)
	{
		NotifyCameraRemoved();

		// The camera should be disabled after NotifyCameraRemoved.
		OP_ASSERT(!m_enabled && "Not all camera users have unregistered.");
		m_camera = NULL;
	}
}

#endif // MEDIA_CAMERA_SUPPORT
