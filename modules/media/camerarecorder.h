/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2010-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef MEDIA_CAMERARECORDER_H
#define MEDIA_CAMERARECORDER_H

#ifdef MEDIA_CAMERA_SUPPORT

#include "modules/media/media.h"
#include "modules/logdoc/complex_attr.h"
#include "modules/pi/device_api/OpCamera.h"
#include "modules/media/src/controls/cameracontrols.h"
#include "modules/device_api/OpListenable.h"

class VisualDevice;
class FramesDocument;

/** A listener for notifying about camera being detached. */
class CameraRemovedListener
{
public:
	/** Called when the current camera is detached. */
	virtual void OnCameraRemoved() = 0;
};

/** A proxy between OpCamera and Core.
 *
 * It provides functions for initiating capture and acts as a splitter
 * for preview notifications. It manages its enabled state to ensure
 * it's always enabled when needed.
 *
 * CameraRecorder supports only one camera (or none), regardless of the number
 * of cameras in the system.
 * The camera that is used is the first one available (as provided by
 * OpCameraManager::GetCameraDevices during initialization or the first one
 * provided by a call to OnCameraAttached) and there is no way to change the
 * active camera.
 * Proper support for multiple cameras will be implemented by CORE-38706.
 */
class CameraRecorder
	: public OpCameraPreviewListener
	, public OpCameraDeviceListener
#ifdef MEDIA_CAMERA_CAPTURE_SUPPORT
	, public OpCameraCaptureListener
#endif // MEDIA_CAMERA_CAPTURE_SUPPORT
	, private OpListenable<OpCameraPreviewListener>
	, private OpListenable<CameraRemovedListener>
{
public:
	/** Create a new CameraRecorder object. */
	static OP_CAMERA_STATUS Create(CameraRecorder*& new_camera_recorder);
	virtual ~CameraRecorder();

#ifdef MEDIA_CAMERA_CAPTURE_SUPPORT
	/** Capture image
	 * @see OpCamera
	 */
	OP_CAMERA_STATUS CaptureImage(OpString* file_name, BOOL lowRes, OpCameraCaptureListener* listener);

	/** Start video capture
	 * @see OpCamera
	 */
	OP_CAMERA_STATUS CaptureVideo(OpString* file_name, UINT32 requested_capture_time, BOOL low_res, OpCameraCaptureListener* listener);

	/** Stop video capture
	 * @see OpCamera
	 */
	OP_CAMERA_STATUS StopVideoCapture();

	/** Pause or resume video capture
	 * @see OpCamera
	 */
	OP_CAMERA_STATUS PauseVideoCapture(BOOL pause);

	/** Resume video capture
	 * @see OpCamera
	 */
	OP_CAMERA_STATUS ResumeVideoCapture();

	/** Aborts any capture that is currently ongoing and is notified to the given listener.
	 *
	 * No more events will be notified to this listener.
	 * @param listener Listener whose capture is aborted.
	 * @see OpCamera
	 */
	void AbortCapture(OpCameraCaptureListener* listener);

	/** Is the camera currently recording? */
	BOOL IsRecording() { return m_recording_video; }

	/** Is the camera currently paused? */
	BOOL IsRecordingPaused() { return m_recording_video_paused; }
#endif // MEDIA_CAMERA_CAPTURE_SUPPORT

	/** Get the current preview frame. */
	OP_CAMERA_STATUS GetCurrentPreviewFrame(OpBitmap*& frame);

	/** Get the size of the current preview frame. */
	OP_CAMERA_STATUS GetPreviewDimensions(unsigned int& width, unsigned int& height);

	/** Attaches a preview listener for the camera.
	 *
	 * Enables the camera if it was disabled.
	 * @return
	 *  - OK - if enabled successfully,
	 *  - ERR_NOT_AVAILABLE - camera is not available, e.g. used by another
	 *  - ERR_NO_MEMORY.
	 */
	OP_CAMERA_STATUS AttachPreviewListener(OpCameraPreviewListener* listener);

	/** Detaches a preview listener for the camera.
	 *
	 * Disables the camera if there are no other listeners
	 * and no operation is taking place.
	 */
	void DetachPreviewListener(OpCameraPreviewListener* listener);

	/** Attaches a camera removal listener.
	 *
	 * @return
	 *  - OK - if successful,
	 *  - ERR_NO_MEMORY - no memory.
	 */
	OP_STATUS AttachCameraRemovedListener(CameraRemovedListener* listener) { return OpListenable<CameraRemovedListener>::AttachListener(listener); }

	/** Detaches a camera removal listener. */
	void DetachCameraRemovedListener(CameraRemovedListener* listener) { OpListenable<CameraRemovedListener>::DetachListener(listener); }

	/** TODO: this is just a stub - when OpCamera will be extended we will use OpCamera here
	 *  in order to obtain proper functionality */
	BOOL IsPauseSupported() { return FALSE; }

	/** Checks if there is a camera in the system. */
	BOOL HasCamera() { return GetCamera() != NULL; }
private:
	CameraRecorder();
	/** Initialize CameraRecorder. */
	OP_STATUS Init();

	OpCamera* GetCamera() const { return m_camera; }

	/** @return TRUE if no operation is ongoing. */
	BOOL IsCameraAvailable();

	/** Enables the camera if it is not yet enabled.
	 *
	 * @return
	 *  - OpStatus::OK - if camera was enabled successfully or is alreasdy enabled.
	 *  - relevant OpCameraStatus if enabling failed.
	 */
	OP_CAMERA_STATUS EnableCamera();

	/** Disables the camera if it is no longer needed.
	 *
	 * Not needed means nothing listens for preview and no capture is
	 * ongoing.
	 */
	void DisableCameraIfNotNeeded();
	BOOL IsPreviewNeeded() { return OpListenable<OpCameraPreviewListener>::HasListeners(); }

#ifdef MEDIA_CAMERA_CAPTURE_SUPPORT
	/* from OpCameraCaptureListener */
	virtual void OnCameraCaptured(OpCamera* camera, const uni_char* file_name);
	virtual void OnCameraCaptureFailed(OpCamera* camera, OP_CAMERA_STATUS error);
	// helper for implementing OpCameraCaptureListener
	void FinishCaptureOperation(OpCamera* camera, OP_CAMERA_STATUS error, const uni_char* file_name);
#endif // MEDIA_CAMERA_CAPTURE_SUPPORT

	/* from OpCameraPreviewListener */
	virtual void OnCameraUpdateFrame(OpCamera* camera);
	virtual void OnCameraFrameResize(OpCamera* camera);

	/* from OpCameraDeviceListener */
	virtual void OnCameraAttached(OpCamera* camera);
	virtual void OnCameraDetached(OpCamera* camera);

	/// defines NotifyCameraUpdateFrame(OpCamera*) method which
	/// safely notifies all PreviewListeners about frame update
	DECLARE_LISTENABLE_EVENT1(OpCameraPreviewListener, CameraUpdateFrame, OpCamera*);

	/// defines NotifyCameraFrameResize(OpCamera*) method which safely
	/// notifies all PreviewListeners about frame dimensions being changed
	DECLARE_LISTENABLE_EVENT1(OpCameraPreviewListener, CameraFrameResize, OpCamera*);

	/// define NotifyCameraAttached() method which safely notifies all
	/// CameraRemovedListeners about a camera removed from the system
	DECLARE_LISTENABLE_EVENT0(CameraRemovedListener, CameraRemoved);

	/** Camera object delivering captured image. */
	OpCamera* m_camera;

	/** Is the camera enabled? */
	unsigned m_enabled:1;

#ifdef MEDIA_CAMERA_CAPTURE_SUPPORT
	/** Listener for ongoing capture operation. */
	OpCameraCaptureListener* m_current_capture_listener;

	/** Is recording paused? */
	unsigned m_recording_video:1;

	/** Is recording paused? */
	unsigned m_recording_video_paused:1;

	/** Is recording paused? */
	unsigned m_capturing_image:1;

	/** Can the camera pause/resume recording? */
	unsigned m_camera_supports_pause:1;
#endif // MEDIA_CAMERA_CAPTURE_SUPPORT
};

#endif // MEDIA_CAMERA_SUPPORT

#endif // !MEDIA_CAMERARECORDER_H
