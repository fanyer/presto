/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2010-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef MEDIA_CAMERAPREVIEW_H
#define MEDIA_CAMERAPREVIEW_H

#ifdef MEDIA_CAMERA_SUPPORT

#include "modules/media/media.h"
#include "modules/logdoc/complex_attr.h"
#include "modules/pi/device_api/OpCamera.h"
#include "modules/media/src/controls/cameracontrols.h"

class VisualDevice;
class FramesDocument;
class CameraRecorder;

class CameraPreview
	: public Media
	, public OpCameraPreviewListener
	, public CameraControlsOwner
{
public:
	CameraPreview(HTML_Element* elm);
	virtual ~CameraPreview();

	/** The aspect ratio adjusted width of the video. */
	virtual UINT32 GetIntrinsicWidth() { return m_last_width; }

	/** The aspect ratio adjusted height of the video. */
	virtual UINT32 GetIntrinsicHeight() { return m_last_height; }

	/** Is this image-like content? */
	virtual BOOL IsImageType() const { return TRUE; }

	/** Paint the current video frame, if any. */
	virtual OP_STATUS Paint(VisualDevice* vis_dev, OpRect video, OpRect content);

	/** Handle mouse event. */
	virtual void OnMouseEvent(DOM_EventType event_type, MouseButton button, int x, int y);

	/** Is the media element's delaying-the-load-event flag set? */
	virtual BOOL GetDelayingLoad() const { return FALSE; }

	/** Called when media controls gain keyboard focus. */
	virtual void HandleFocusGained();

	/** Focus next internal media control. */
	virtual BOOL FocusNextInternalTabStop(BOOL forward);

	/** Focus first internal media control when tab navigation enters element. */
	virtual void FocusInternalEdge(BOOL forward);

	/** Is the media element focusable? */
	virtual BOOL IsFocusable() const { return m_controls != NULL; }

	/** Stop playback and free as many resources as possible. */
	virtual void Suspend(BOOL removed);

	/** Resume state from before Suspend was called. */
	virtual OP_STATUS Resume();

#ifdef MEDIA_CAMERA_CAPTURE_SUPPORT
	/** Invoked when user selected 'record' control. */
	virtual void OnMultiFuncButton();

	/** Invoked when user selected 'stop recording' control. */
	virtual void OnCameraStop();
#endif // MEDIA_CAMERA_CAPTURE_SUPPORT

	/** Invoked when Camera controls need information about the camera used. */
	virtual BOOL GetCameraSupportsPause();

	/** Start or resume previously paused playback. */
	OP_CAMERA_STATUS EnablePreview();

	/** Disable preview. */
	void DisablePreview();

#ifdef MEDIA_CAMERA_CAPTURE_SUPPORT
	struct RecordingParams
	{
		OpString filename;
		OpCameraCaptureListener* listener;
		BOOL low_res;
		unsigned int max_duration_milliseconds;
	};
	/** Enable camera controls. */
	OP_STATUS EnableControls(BOOL enable, const RecordingParams& recording_params);
#endif // MEDIA_CAMERA_CAPTURE_SUPPORT

	/** Should controls be displayed? */
	virtual BOOL ControlsNeeded() const { return FALSE; /* Controls are not yet supported */ }

private:
#ifdef MEDIA_CAMERA_CAPTURE_SUPPORT
	/** Pause or resume video capture. */
	OP_CAMERA_STATUS PauseVideoCapture(BOOL pause);

	/** Stop video capture. */
	OP_CAMERA_STATUS StopVideoCapture();

	/** Starts video capture using the record parameters passed when enabling controls. */
	OP_CAMERA_STATUS StartVideoCapture();
#endif // MEDIA_CAMERA_CAPTURE_SUPPORT

	/** Camera frame update listener. */
	virtual void OnCameraUpdateFrame(OpCamera* camera);
	/** Camera frame size change listener. */
	virtual void OnCameraFrameResize(OpCamera* camera);

	/** Get FramesDocument. */
	FramesDocument* GetFramesDocument();

	/** Control camera controls depending on element state. */
	OP_STATUS ManageCameraControls();

	/** Invalidate camera element. */
	void Update();

	/* Camera controls */
	CameraControls* m_controls;

	/* Is camera enabled? */
	unsigned m_enabled:1;

	/* Shoud controls be displayed? */
	unsigned m_has_controls:1;

	/* Set to TRUE when recording is paused */
	unsigned m_recording_paused:1;

	/* Previous frame's width */
	UINT32 m_last_width;

	/* Previous frame's height */
	UINT32 m_last_height;

#ifdef MEDIA_CAMERA_CAPTURE_SUPPORT
	RecordingParams m_suspended_recording;
#endif // MEDIA_CAMERA_CAPTURE_SUPPORT
};

#endif // MEDIA_CAMERA_SUPPORT

#endif // MEDIA_CAMERAPREVIEW_H
