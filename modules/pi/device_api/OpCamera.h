/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef PI_DEVICE_API_OPCAMERA_H
#define PI_DEVICE_API_OPCAMERA_H

#ifdef PI_CAMERA

typedef OP_STATUS OP_CAMERA_STATUS;
#ifdef PI_CAMERA_CAPTURE
class OpCameraCaptureListener;
#endif // PI_CAMERA_CAPTURE
class OpCameraPreviewListener;

/** @short An OpStatus extension for use with OpCamera interface.
 */
class OpCameraStatus : public OpStatus
{
public:
	enum
	{
		/** Operation has been aborted (possibly by the user) */
		ERR_OPERATION_ABORTED = USER_ERROR + 1,

		/** File already exists (and cannot be overwritten) */
		ERR_FILE_EXISTS,

		/** Camera is not available (e.g. busy or turned off), but might be
		 *  available some other time */
		ERR_NOT_AVAILABLE,

		/** Requested resolution is not supported or invalid */
		ERR_RESOLUTION_NOT_SUPPORTED,

		/** Operation cannot be performed because the camera has not been enabled */
		ERR_DISABLED
	};
};

/** @short Interface for handling the photo camera.
 *
 * The camera needs to be enabled with Enable before any capture functions
 * are used. It should also be disabled with Disable when no longer needed.
 * This allows other parts of the application to take hold of the camera.
 *
 * The camera supports preview (with the OpCameraPreviewListener and
 * GetCurrentFrame function), still image capture (to file) and video capture
 * (to a video file).
 */
class OpCamera
{
public:
	struct Resolution
	{
		int x, y; //< width and height of camera output in pixels

		Resolution() : x(0), y(0) { }
	};

#ifdef PI_CAMERA_CAPTURE
	/** Additional parameters for capture methods
	 */
	struct CaptureParams
	{	/** Resolution of captured image/video. */
		Resolution resolution;

		/** Max duration of captured video in ms.
			Only for video capture. 0 means unlimited duration
		 */
		UINT32 max_duration_miliseconds;
		CaptureParams() 
			: max_duration_miliseconds(0)
		{}
	};
#endif // PI_CAMERA_CAPTURE

protected:
	// Protected - don't delete these objects from core.
	virtual ~OpCamera(){};

public:

	/** Enables the camera preview.
	 *
	 * Once the camera is enabled, frames will be available through the
	 * GetCurrentFrame method.
	 * The resolution of the preview is up to the platform implementation.
	 *
	 * @param preview_listener Object to be notified of preview updates. May be NULL.
	 *                         If provided, the object must exist until Disable is called.
	 *
	 * @return
	 *  - OK - if enabled successfully,
	 *  - ERR_NOT_AVAILABLE - camera is not available, e.g. used by another
	 *  - ERR_NO_MEMORY
	 *	- ERR - other, unspecified and unrecoverable error
	 */
	virtual OP_CAMERA_STATUS Enable(OpCameraPreviewListener* preview_listener) = 0;

	/** Turns off camera and the preview.
	 *
	 * If any recording operation is in progress, it should be aborted (and
	 * appropriate callbacks should be called, just as if any other error has
	 * occurred, with the ERR_DISABLED error code).
	 *
	 * Note: the preview_listener provided to Enable is not valid after Disable
	 * has been called.
	 *
	 * The operation may be asynchronous, i.e. the actual device may be disabled
	 * after this function returns.
	 */
	virtual void Disable() = 0;

	/** Gets current preview frame from the camera.
	 *
	 * The camera needs to be enabled for the preview to work (see Enable).
	 *
	 * @param[out] frame Set to point to a bitmap containing the current frame.
	 * The frame has to be immediately copied. The resolution of the preview
	 * frame is up to the platform code.
	 *
	 * @return
	 *	- OK
	 *	- ERR_DISABLED - if the camera has not been enabled
	 *  - ERR_NO_MEMORY
	 *	- ERR - other, unspecified and unrecoverable error
	 */
	virtual OP_CAMERA_STATUS GetCurrentPreviewFrame(OpBitmap** frame) = 0;

	/** Gets current resolution of the preview frames.
	 *
	 *  The frame dimensions may change on orientation change.
	 *
	 *  @param[out] dimensions Set to point to current dimensions of the
	 *  preview frame.
	 *  @return
	 *   - OK
	 *   - ERR_DISABLED - if the camera has not been enabled
	 *   - ERR_NO_MEMORY
	 *   - ERR - other, unspecified and unrecoverable error
	 *
	 *  @see OpCameraPreviewListener::OnCameraFrameResize
	 */
	virtual OP_CAMERA_STATUS GetPreviewDimensions(Resolution* dimensions) = 0;

#ifdef PI_CAMERA_CAPTURE
	/** Starts video capture.
	 *
	 * Starts video recording. The function may be provided with a suggested
	 * destination filename. If possible, this name should be used.
	 * The file_name argument must be set to the absolute path name of the
	 * destination file (whether the suggested name is used or not).
	 *
	 * The file name passed as argument to the listener's OnCameraCaptured
	 * must be identical as the one file_name is set to.
	 *
	 * No files should be overwritten by this function.
	 *
	 * The recording may be finished with StopVideoCapture and the listener is
	 * notified when recording stops.
	 *
	 * May only be called when the camera is enabled (see Enable).
	 *
	 * @param[in,out] file_name Name of the file to which the video will be
	 *                saved. May be an empty string, which means the function
	 *                should choose a name.
	 *                It should be set to the full path to the file that will
	 *                store the recording.
	 * @param listener Listener object to be notified when capture finishes. May
	 *        be called only if the function returns OpStatus::OK.
	 *        Methods on the listener may be called before the function returns.
	 * @param params Capture parameters.
	 *
	 * @return
	 *  - OK
	 *  - ERR_FILE_EXISTS - if file already exists.
	 *  - ERR_ERR_RESOLUTION_NOT_SUPPORTED - if the provided resolution is invalid.
	 *  - ERR_DISABLED - if the camera hasn't been enabled.
	 *  - ERR_NO_MEMORY
	 *	- ERR - other, unspecified and unrecoverable error
	 */
	virtual OP_CAMERA_STATUS StartVideoCapture(OpString* file_name, OpCameraCaptureListener* listener, const CaptureParams& params) = 0;

	/** Stops the video capture.
	 *
	 * Signals the device to stop recording. This function may return before
	 * recording has actually stopped. The listener that has been passed to
	 * StartCapture will be notified after recording has stopped.
	 *
	 * @return
	 *  - OK
	 *  - ERR - no video is being captured.
	 */
	virtual OP_CAMERA_STATUS StopVideoCapture() = 0;

	/** @short Starts still image capture.
	 *
	 * Captures a photo to an image file. The function may be provided with a
	 * suggested destination filename. If possible, this name should be used.
	 * The file_name argument must be set to the absolute path name of the
	 * destination file (whether the suggested name is used or not).
	 *
	 * The file name passed as argument to the listener's OnCameraCaptured
	 * must be identical as the one file_name is set to.
	 *
	 * No files should be overwritten by this function.
	 *
	 * May only be called when the camera is enabled (see Enable).
	 *
	 * @param[in,out] file_name Name of the file to which the photo will be
	 *                saved. May be an empty string, which means the function
	 *                should choose a name.
	 *                It should be set to the full path to the file that will
	 *                store the photo.
	 * @param listener Listener object to be notified when capture finishes. May
	 *        be called only if the function returns OpStatus::OK.
	 *        Methods on the listener may be called before the function returns.
	 * @param params Capture parameters.
	 * @return
	 *  - OK - if file_name has been set and image capture operation initialized.
	 *  - ERR_FILE_EXISTS - if file already exists.
	 *  - ERR_ERR_RESOLUTION_NOT_SUPPORTED - if the provided resolution is invalid.
	 *  - ERR_DISABLED - if the camera hasn't been enabled.
	 *  - ERR_NO_MEMORY
	 *	- ERR - other, unspecified and unrecoverable error
	 */
	virtual OP_CAMERA_STATUS CaptureImage(OpString* file_name, OpCameraCaptureListener* listener, const CaptureParams& params) = 0;

	/** Gets all the resolutions supported by this camera for still images.
	 *
	 * Note: this functionality must always be supported, even if the camera is
	 * turned off or in use by other application (it could be obtained at the
	 * start and cached).
	 * Only if it is absolutely impossible to achieve, the function may return
	 * ERR_NOT_AVAILABLE.
	 *
	 * @param[out] resolutions List of resolutions, sorted ascending by width
	 * dimension.
	 * At least one value has to be provided if OK is returned.
	 *
	 * @return
	 *  - OpStatus::OK
	 *  - OpStatus::ERR_NO_MEMORY
	 *  - OpStatus::ERR_NOT_SUPPORTED - the camera doesn't support image capture
	 *  - OpStatus::ERR_NOT_AVAILABLE - if it is absolutely impossible to
	 *    obtain the information right now
	 */
	virtual OP_CAMERA_STATUS GetSupportedImageResolutions(OpAutoVector<Resolution>* resolutions) = 0;

	/** Gets all the resolutions supported by this camera for video capture.
	 *
	 * It may be possible for a camera to only support capturing still images.
	 * Therefore it is possible for this function to return
	 * OpStatus::ERR_NOT_SUPPORTED in such a case.
	 *
	 * Note: this functionality must always be supported, even if the camera is
	 * turned off or in use by other application (it could be obtained at the
	 * start and cached).
	 * Only if it is absolutely impossible to achieve, the function may return
	 * ERR_NOT_AVAILABLE.
	 *
	 * @param[out] resolutions List of resolutions, sorted ascending by width
	 * dimension. At least one value has to be provided if OK is returned.
	 *
	 * @return
	 *  - OpStatus::OK
	 *  - OpStatus::ERR_NO_MEMORY
	 *  - OpStatus::ERR_NOT_SUPPORTED - the camera doesn't support video capture
	 *  - OpStatus::ERR_NOT_AVAILABLE - if it is absolutely impossible to
	 *    obtain the information right now
	 */
	virtual OP_CAMERA_STATUS GetSupportedVideoResolutions(OpAutoVector<Resolution>* resolutions) = 0;

	/** Gets default resolution of this camera for image shooting.
	 *
	 * The default resolution is the one preferred by the manufacturer or
	 * configured by the user.
	 *
	 * Note: this functionality must always be supported, even if the camera is
	 * turned off or in use by other application (it could be obtained at the
	 * start and cached).
	 * Only if it is absolutely impossible to achieve, the function may return
	 * ERR_NOT_AVAILABLE.
	 *
	 * @param[out] resolution Set to the default resolution.
	 *
	 * @return
	 *  - OpStatus::OK
	 *  - OpStatus::ERR_NO_MEMORY
	 *  - OpStatus::ERR_NOT_SUPPORTED - the camera doesn't support image capture
	 *  - OpStatus::ERR_NOT_AVAILABLE - if it is absolutely impossible to
	 *    obtain the information right now
	 */
	virtual OP_CAMERA_STATUS GetDefaultImageResolution(Resolution* resolution) = 0;

	/** Gets default resolution of this camera for video capture.
	 *
	 * The default resolution is the one preferred by the manufacturer or
	 * configured by the user.
	 *
	 * It may be possible for a camera to only support capturing still images.
	 * Therefore it is possible for this function to return
	 * OpStatus::ERR_NOT_SUPPORTED in such a case.
	 *
	 * Note: this functionality must always be supported, even if the camera is
	 * turned off or in use by other application (it could be obtained at the
	 * start and cached).
	 * Only if it is absolutely impossible to achieve, the function may return
	 * ERR_NOT_AVAILABLE.
	 *
	 * @param[out] resolution Set to the default resolution.
	 *
	 * @return
	 *  - OpStatus::OK
	 *  - OpStatus::ERR_NO_MEMORY
	 *  - OpStatus::ERR_NOT_SUPPORTED - the camera doesn't support video capture
	 *  - OpStatus::ERR_NOT_AVAILABLE - if it is absolutely impossible to
	 *    obtain the information right now
	 */
	virtual OP_CAMERA_STATUS GetDefaultVideoResolution(Resolution* resolution) = 0;
#endif // PI_CAMERA_CAPTURE
};

#ifdef PI_CAMERA_CAPTURE
/** @short A listener to be notified when image or video capture has finished.
 */
class OpCameraCaptureListener
{
public:
	/** Notifies of successful capture finish.
	 *
	 * This function is called whenever anything has been recorded
	 * (e.g. if during video recording the storage runs out of space but a
	 * correct video file has been saved).
	 *
	 * @param file_name Full path to the file containing the captured data
	 * (the string is copied).
	 */
	virtual void OnCameraCaptured(OpCamera* camera, const uni_char* file_name) = 0;

	/** Notifies of capture failure.
	 *
	 * This function should be called only if no recording has been made,
	 * otherwise OnCameraCaptured should be called.
	 *
	 * @param error Reason of failure or OpStatus::ERR for general error
	 *  - ERR_FILE_EXISTS - destination file exists and cannot be overwritten,
	 *  - ERR_OPERATION_ABORTED - operation has been aborted,
	 *  - ERR_NOT_AVAILABLE - camera currently unavailable,
	 *  - ERR_FILE_NOT_FOUND - destination path is invalid (e.g. specifies a file in nonexisting directory)
	 *  - ERR_NO_DISK - disk is full (and no file has been saved)
	 *  - ERR_DISABLED - the camera has been disabled
	 *  - ERR_NO_MEMORY
	 */
	virtual void OnCameraCaptureFailed(OpCamera* camera, OP_CAMERA_STATUS error) = 0;
};
#endif // PI_CAMERA_CAPTURE

/** @short A listener to be notified of a new preview frame being available.
 */
class OpCameraPreviewListener
{
public:
	/** To be called whenever a new preview frame is available.
	 */
	virtual void OnCameraUpdateFrame(OpCamera* camera) = 0;

	/** To be called whenever the dimensions of the preview change.
	 *
	 * The change may be due to e.g. orientation change.
	 * The method should be called before the frame with new dimensions is
	 * available.
	 */
	virtual void OnCameraFrameResize(OpCamera* camera) = 0;
};

/** @short A listener for notifications about cameras being added or removed from the system.
 */
class OpCameraDeviceListener
{
public:
	/* To be called whenever a new camera is added to the system.
	 */
	virtual void OnCameraAttached(OpCamera* camera) = 0;

	/* To be called whenever a camera is removed from the system.
	 *
	 * @param camera The camera being detached. The camera object may be
	 * deleted after calling this method.
	 */
	virtual void OnCameraDetached(OpCamera* camera) = 0;
};

/** @short Provides access to camera devices.
 *
 * OpCameraManager owns all the camera objects.
 *
 * In Core there is a CameraRecorder object that implements
 * OpCameraDeviceListener. All users of the OpCamera objects should implement
 * the CameraRemovedListener and attach to CameraRecorder to avoid using an
 * OpCamera after it has been deleted.
 */
class OpCameraManager
{
public:
	virtual ~OpCameraManager() {}

	/** Create a new OpCameraManager object.
	 *
	 * @param[out] new_camera_manager Placeholder for newly created
	 * OpCameraManager object.
	 *
	 * @return OpStatus::OK, OpStatus::ERR_NOT_SUPPORTED or OpStatus::ERR_NO_MEMORY.
	 */
	static OP_STATUS Create(OpCameraManager** new_camera_manager);

	/** Return all camera devices currently available.
	 *
	 * @param camera_devices Vector to be filled in with all devices
	 * available in the system. The OpCamera objects are owned by the
	 * OpCameraManager.
	 *
	 * @return OK, ERR_NO_MEMORY or ERR_NOT_SUPPORTED.
	 */
	virtual OP_STATUS GetCameraDevices(OpVector<OpCamera>* camera_devices) = 0;

	/** Set the listener for camera device changes.
	 *
	 * Calling this method will remove the previously set listener.
	 *
	 * @param camera_device_listener The new listener to be notified of cameras
	 * being added to or removed from the system. May be NULL.
	 * @see CameraRecorder
	 */
	virtual void SetCameraDeviceListener(OpCameraDeviceListener* camera_device_listener) = 0;
};

#endif // PI_CAMERA

#endif // PI_DEVICE_API_OPCAMERA_H
