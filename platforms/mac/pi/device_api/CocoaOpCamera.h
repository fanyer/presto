/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef COCOA_OPCAMERA_H
#define COCOA_OPCAMERA_H

#ifdef PI_CAMERA
#define BOOL NSBOOL
#import <Cocoa/Cocoa.h>
#ifndef __LP64__
#define TLP64
#define __LP64__ 1
#endif
#import <QTKit/QTKit.h>
#ifdef TLP64
#undef __LP64__
#endif
#undef BOOL
#include "modules/pi/device_api/OpCamera.h"

#include <CoreFoundation/CoreFoundation.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>

class CocoaOpCamera;
class CocoaOpCameraManager;

@interface CameraPreviewListener : NSObject
{
	OpCameraPreviewListener* _preview_listener;
	CocoaOpCamera* _camera;
	CVImageBufferRef _last_frame;
	OpBitmap* _bitmap;
}

-(id) initWithCamera:(CocoaOpCamera*)camera listener:(OpCameraPreviewListener*) _preview_listener;
- (OpBitmap*) getLastFrame;
@end

#ifdef PI_CAMERA_CAPTURE
@interface CameraCaptureListener : NSObject
{
	OpCameraCaptureListener* _capture_listener;
	CocoaOpCamera* _camera;
}

-(id) initWithCamera:(CocoaOpCamera*)camera capture_listener:(OpCameraCaptureListener*) capture_listener;
@end
#endif

@interface CameraConnectedObserver : NSObject
{
    CocoaOpCameraManager* _parent;
    CocoaOpCamera* _camera;
}

-(id) initWithManager: (CocoaOpCameraManager*) parent andCamera: (CocoaOpCamera*) camera;
@end

class CocoaOpCamera : public OpCamera, public MessageObject
{
public:
	CocoaOpCamera(QTCaptureDevice* device, CocoaOpCameraManager* parent);
	~CocoaOpCamera();
	OP_STATUS Init();
	virtual OP_CAMERA_STATUS Enable(OpCameraPreviewListener* preview_listener);
	virtual void Disable();
	virtual OP_CAMERA_STATUS GetCurrentPreviewFrame(OpBitmap** frame);
	virtual OP_CAMERA_STATUS GetPreviewDimensions(Resolution* dimensions);
#ifdef PI_CAMERA_CAPTURE
	virtual OP_CAMERA_STATUS StartVideoCapture(OpString* file_name, OpCameraCaptureListener* listener, const CaptureParams& params);
	virtual OP_CAMERA_STATUS StopVideoCapture();
	virtual OP_CAMERA_STATUS CaptureImage(OpString* file_name, OpCameraCaptureListener* listener, const CaptureParams& params);
	virtual OP_CAMERA_STATUS GetSupportedImageResolutions(OpAutoVector<Resolution>* resolutions);
	virtual OP_CAMERA_STATUS GetSupportedVideoResolutions(OpAutoVector<Resolution>* resolutions);
	virtual OP_CAMERA_STATUS GetDefaultImageResolution(Resolution* resolution);
	virtual OP_CAMERA_STATUS GetDefaultVideoResolution(Resolution* resolution);
#endif // PI_CAMERA_CAPTURE
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
    
    //Parent API

    bool IsConnected() { return m_device && [m_device isConnected]; }
    bool Contains(QTCaptureDevice* device);
    void OnConnectedChange(CocoaOpCameraManager* parent);
private:
    
	QTCaptureDevice* m_device;
	QTCaptureSession *m_captureSession;
	QTCaptureMovieFileOutput *m_captureFile;
	QTCaptureVideoPreviewOutput *m_capturePreview;
	QTCaptureDeviceInput *m_captureDevice;
    CameraConnectedObserver* m_connectedObserver;
	OpCameraPreviewListener* m_preview_listener;
    bool m_wasConnected;
};

class CocoaOpCameraManager : public OpCameraManager
{
    OpCameraDeviceListener* m_device_listener;
    OpAutoVector<CocoaOpCamera> m_cameras;     // all cameras, as seen from the start of Opera, including those that are currently disconnected
    IONotificationPortRef m_notifyPort;
    io_iterator_t m_notifyIter;

    inline OP_STATUS StartNotificationPort();
	inline void UpdateCameraList(bool notify = true);
    
    class IONotification; //declared and implemented in .mm

public:
	CocoaOpCameraManager(): m_device_listener(NULL), m_notifyPort(NULL), m_notifyIter(NULL) {}
	virtual ~CocoaOpCameraManager();
	virtual OP_STATUS GetCameraDevices(OpVector<OpCamera>* camera_devices);
    virtual void SetCameraDeviceListener(OpCameraDeviceListener* camera_device_listener);

    void CameraAttached(CocoaOpCamera* camera)
    {
        if (m_device_listener)
            m_device_listener->OnCameraAttached(camera);
    }
    
    void CameraDetached(CocoaOpCamera* camera)
    {
        if (m_device_listener)
            m_device_listener->OnCameraDetached(camera);
    }
};


#endif // PI_CAMERA
#endif // COCOA_OPCAMERA_H
