/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef PI_CAMERA
#include "platforms/mac/pi/device_api/CocoaOpCamera.h"
#include "modules/pi/OpThreadTools.h"
#include "modules/pi/OpBitmap.h"

OP_STATUS OpCameraManager::Create(OpCameraManager** new_camera_manager)
{
	*new_camera_manager = OP_NEW(CocoaOpCameraManager, ());
	return *new_camera_manager ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

CocoaOpCameraManager::~CocoaOpCameraManager()
{
    if (m_notifyIter)
    {
        IOObjectRelease(m_notifyIter);
    }
    
    if (m_notifyPort)
    {
        IONotificationPortDestroy(m_notifyPort);
        m_notifyPort = NULL;
    }
}

OP_STATUS CocoaOpCameraManager::GetCameraDevices(OpVector<OpCamera>* camera_devices)
{
    if (!m_notifyPort)
        StartNotificationPort();

	UINT32 count = m_cameras.GetCount();
	for (UINT32 i = 0; i < count; ++i)
	{
		CocoaOpCamera* cam = m_cameras.Get(i);
        OP_ASSERT(!cam || (cam && !cam->IsConnected()));
        
		if (cam && cam->IsConnected())
			camera_devices->Add(cam);
	}
    
	return OpStatus::OK;
}

void CocoaOpCameraManager::SetCameraDeviceListener(OpCameraDeviceListener* camera_device_listener)
{
    m_device_listener = camera_device_listener;
}

inline void CocoaOpCameraManager::UpdateCameraList(bool notify)
{
    UINT32 count = m_cameras.GetCount();

    NSArray* devs = [QTCaptureDevice inputDevicesWithMediaType:QTMediaTypeVideo];

	QTCaptureDevice* device;
	for (device in devs) {

        CocoaOpCamera* found = NULL;
        
        //Look the device up first - we might have already heard about it.
        for (UINT32 i = 0; i < count; ++i)
        {
            CocoaOpCamera* cam = m_cameras.Get(i);
            if (cam && cam->Contains(device))
            {
                found = cam;
                break;
            }
        }

        if (found)
        {
            // if we found a device, we are done with this one.
            continue;
        }

        //we have a new device attached
		OpAutoPtr<CocoaOpCamera> cam( OP_NEW(CocoaOpCamera, (device, this)) );

        if (OpStatus::IsSuccess(cam.get()->Init()) &&
            OpStatus::IsSuccess(m_cameras.Add(cam.get())))
        {
            if (notify)
                CameraAttached(cam.get());
            cam.release();
        }
	}
}

class CocoaOpCameraManager::IONotification
{
    io_object_t notification;
    
    static inline void UpdateCameraList()
    {
        if (g_op_camera_manager)
            static_cast<CocoaOpCameraManager*>(g_op_camera_manager)->UpdateCameraList();
    }
    
    static inline IONotificationPortRef NotifyPort()
    {
        return g_op_camera_manager ?
            static_cast<CocoaOpCameraManager*>(g_op_camera_manager)->m_notifyPort:
            NULL;
    }
    
    static void IODeviceNotification(void *refCon, io_service_t service, natural_t messageType, void *messageArgument)
    {
        if (messageType == kIOMessageServiceIsTerminated) {
            IONotification* _this = reinterpret_cast<IONotification*>(refCon);
            OP_DELETE(_this);
            UpdateCameraList();
        }
    }
    
public:
    
    IONotification(): notification(NULL) {}
    ~IONotification()
    {
        if (notification)
            IOObjectRelease(notification);
    }
    
    kern_return_t Init(io_service_t device)
    {
        // Register for an interest notification of this device being removed. Use a reference
        // to this object as the refCon which will be passed to the notification callback.
        return IOServiceAddInterestNotification(NotifyPort(), device, kIOGeneralInterest, IODeviceNotification, this, &notification);
    }
    
    static void DevicesAdded(void *, io_iterator_t iterator)
    {
        OpStatus::Ignore(OnDevicesAdded(iterator));
    }

    static OP_STATUS OnDevicesAdded(io_iterator_t iter, bool update = true);
};

inline OP_STATUS CocoaOpCameraManager::StartNotificationPort()
{
    UpdateCameraList(false);

    CFMutableDictionaryRef matchingDict = IOServiceMatching(kIOUSBDeviceClassName);	// Interested in instances of class

    if (matchingDict == NULL)
        return OpStatus::ERR;

    // Create a notification port and add its run loop event source to our run loop
    // This is how async notifications get set up.
    
    m_notifyPort = IONotificationPortCreate(kIOMasterPortDefault);
    CFRunLoopSourceRef runLoopSource = IONotificationPortGetRunLoopSource(m_notifyPort);
    
    CFRunLoopRef runLoop = CFRunLoopGetCurrent();
    CFRunLoopAddSource(runLoop, runLoopSource, kCFRunLoopDefaultMode);
    
    // Now set up a notification to be called when a device is first matched by I/O Kit.
    if (m_notifyIter)
    {
        IOObjectRelease(m_notifyIter);
        m_notifyIter = NULL;
    }
    
    kern_return_t kr = IOServiceAddMatchingNotification(m_notifyPort, kIOFirstMatchNotification, matchingDict,
                                                        IONotification::DevicesAdded, NULL, &m_notifyIter);
    if (kr != KERN_SUCCESS)
        return OpStatus::ERR;
    
    // Iterate once to get already-present devices and arm the notification    
    return IONotification::OnDevicesAdded(m_notifyIter, false);
}

OP_STATUS CocoaOpCameraManager::IONotification::OnDevicesAdded(io_iterator_t iterator, bool update)
{
    kern_return_t		kr;
    io_service_t		usbDevice;
    
    while ((usbDevice = IOIteratorNext(iterator)))
    {
        OpAutoPtr<IONotification> privateData( OP_NEW(IONotification, ()) );

        kr = privateData.get()->Init(usbDevice);
        if (KERN_SUCCESS == kr)
            privateData.release(); //ownership is transferred to the service for now...
        
        // Done with this USB device; release the reference added by IOIteratorNext
        IOObjectRelease(usbDevice);
    }

    if (update)
        UpdateCameraList();

    return OpStatus::OK;
}

@implementation CameraPreviewListener

-(id) initWithCamera:(CocoaOpCamera*)camera listener:(OpCameraPreviewListener*) preview_listener
{
	self = [super init];
	_camera = camera;
	_preview_listener = preview_listener;
	_last_frame = nil;
	_bitmap = NULL;
	return self;
}

-(void)dealloc
{
	@synchronized(self) {
		if (_last_frame)
			CVBufferRelease(_last_frame);
	}
	delete _bitmap;
	[super dealloc];
}

- (void)captureOutput:(QTCaptureOutput *)captureOutput didOutputVideoFrame:(CVImageBufferRef)videoFrame withSampleBuffer:(QTSampleBuffer *)sampleBuffer fromConnection:(QTCaptureConnection *)connection
{
	CVBufferRetain(videoFrame);
	// This function is usually called from a thread. Make sure it works safely with -getLastFrame
	@synchronized(self) {
		if (_last_frame)
			CVBufferRelease(_last_frame);
		_last_frame = videoFrame;
	}
	// Post the message to the main thread
	g_thread_tools->PostMessageToMainThread(MSG_MAC_CAMERA_UPDATE_FRAME, (MH_PARAM_1)_camera, (MH_PARAM_2)NULL);
}

- (OpBitmap*) getLastFrame
{
	CVImageBufferRef frame = NULL;
	// - captureOutput:didOutputVideoFrame:withSampleBuffer:fromConnection may change and release _last_frame.
	// Make sure we use a locally retained version.
	@synchronized(self) {
		frame = _last_frame;
		if (frame)
			CVBufferRetain(frame);
	}
	if (!frame)
		return NULL;

	// Prevent CoreVideo from messing whith the frame data while we work
	CVPixelBufferLockBaseAddress(frame, 0);
	size_t w = CVPixelBufferGetWidth(frame), h = CVPixelBufferGetHeight(frame);

	if (!_bitmap || _bitmap->Width() != w || _bitmap->Height() != h) {
		delete _bitmap;
		_bitmap = NULL;
		if (OpStatus::IsError(OpBitmap::Create(&_bitmap, w, h)))
			return NULL;
	}

	UINT8* source = (UINT8*) CVPixelBufferGetBaseAddress(frame);
	UINT8* dest = (UINT8*) _bitmap->GetPointer(OpBitmap::ACCESS_WRITEONLY);
	for (size_t i=0; i<h; i++) {
		UINT32* line_source = (UINT32*) (source+i*CVPixelBufferGetBytesPerRow(frame));
		UINT32* line_dest = (UINT32*) (dest+i*_bitmap->GetBytesPerLine());
		for (size_t j=0; j<w; j++) {
			*line_dest++ = *line_source++;
		}
	}
	_bitmap->ReleasePointer(TRUE);
	// Now we are done whith messing whith the frame
	CVPixelBufferUnlockBaseAddress(frame, 0);

	// Decref. If captureOutput has released its copy this line will free the data.
	CVBufferRelease(frame);
	return _bitmap;
}

@end

#ifdef PI_CAMERA_CAPTURE
@implementation CameraCaptureListener

-(id) initWithCamera:(CocoaOpCamera*)camera capture_listener:(OpCameraCaptureListener*) capture_listener
{
	self = [super init];
	_camera = camera;
	_capture_listener = capture_listener;
	return self;
}

// This method is called every time the recorder output receives a new sample buffer. When called within this method, recordToOutputFileURL: and recordToOutputFileURL:bufferDestination:, are all guaranteed to occur on the given sample buffer. Delegates should not expect this method to be called on the main thread. In addition, since this method is called frequently, it must be efficient.
- (void)captureOutput:(QTCaptureFileOutput *)captureOutput didOutputSampleBuffer:(QTSampleBuffer *)sampleBuffer fromConnection:(QTCaptureConnection *)connection
{
}

// These methods are called when the output starts recording to a new file.
- (void)captureOutput:(QTCaptureFileOutput *)captureOutput didStartRecordingToOutputFileAtURL:(NSURL *)fileURL forConnections:(NSArray *)connections
{
}

// This method is called whenever a file is finished successfully. If the file was forced to be finished due to an error (including errors that resulted in either of the above two methods being called), the error is described in the error parameter. Otherwise, the error parameter equals nil.
- (void)captureOutput:(QTCaptureFileOutput *)captureOutput didFinishRecordingToOutputFileAtURL:(NSURL *)outputFileURL forConnections:(NSArray *)connections dueToError:(NSError *)error
{
	OpString path;
	if (SetOpString(path, (CFStringRef)[outputFileURL path]))
		_capture_listener->OnCameraCaptured(_camera, path);
}

@end
#endif // PI_CAMERA_CAPTURE

@implementation CameraConnectedObserver

-(id) initWithManager:(CocoaOpCameraManager *)parent andCamera:(CocoaOpCamera *)camera
{
    self = [super init];
    _parent = parent;
    _camera = camera;
    return self;
}

-(void) observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    if (![keyPath isEqualToString:@"connected"])
        return;

    _camera->OnConnectedChange(_parent);
}

@end

CocoaOpCamera::CocoaOpCamera(QTCaptureDevice* device, CocoaOpCameraManager* parent)
	: m_device(device), m_captureSession(NULL), m_captureFile(NULL), m_capturePreview(NULL), m_captureDevice(NULL), m_connectedObserver(NULL), m_preview_listener(NULL), m_wasConnected(true)
{
	[m_device retain];
	m_captureSession = [[QTCaptureSession alloc] init];
    m_connectedObserver = [[CameraConnectedObserver alloc] initWithManager:parent andCamera:this];
    [m_device addObserver:m_connectedObserver forKeyPath:@"connected" options:NSKeyValueObservingOptionOld context:nil];
}

CocoaOpCamera::~CocoaOpCamera()
{
	Disable();
	[m_device release];
    if (m_connectedObserver)
        [m_connectedObserver release];
	if (m_captureDevice)
		[m_captureDevice release];
	if (m_captureFile)
		[m_captureFile release];
	[m_captureSession release];
}

OP_STATUS CocoaOpCamera::Init()
{
	BOOL success = NO;
	NSError *error;
	success = [m_device open:&error];
	if (!success)
		return OpStatus::ERR;

	m_captureDevice = [[QTCaptureDeviceInput alloc] initWithDevice:m_device];
	success = [m_captureSession addInput:m_captureDevice error:&error];
	if (!success)
		return OpStatus::ERR;

	g_main_message_handler->SetCallBack(this, MSG_MAC_CAMERA_UPDATE_FRAME, (MH_PARAM_1)this);

	return OpStatus::OK;
}

OP_CAMERA_STATUS CocoaOpCamera::Enable(OpCameraPreviewListener* preview_listener)
{
	BOOL success = NO;
	NSError *error;
	m_preview_listener = preview_listener;
	if (!m_capturePreview) {
		m_capturePreview = [[QTCaptureVideoPreviewOutput alloc] init];
		[m_capturePreview setPixelBufferAttributes:[NSDictionary dictionaryWithObjectsAndKeys:
			//TODO: Find native resolution, if possible.
			[NSNumber numberWithInt:640], kCVPixelBufferWidthKey,
			[NSNumber numberWithInt:480], kCVPixelBufferHeightKey,
			[NSNumber numberWithUnsignedInt:k32BGRAPixelFormat /*kCVPixelFormatType_32BGRA*/], kCVPixelBufferPixelFormatTypeKey,
			nil]];
		success = [m_captureSession addOutput:m_capturePreview error:&error];
		if (!success)
			return OpStatus::ERR;
	}
	id delegate = [m_capturePreview delegate];
	[m_capturePreview setDelegate:[[CameraPreviewListener alloc] initWithCamera:this listener:preview_listener]];
	if (delegate)
		[delegate release];
	[m_captureSession startRunning];
	return OpStatus::OK;
}

void CocoaOpCamera::Disable()
{
	id delegate;
	m_preview_listener = NULL;
	if ((delegate = [m_capturePreview delegate])) {
		[m_capturePreview setDelegate:nil];
		[delegate release];
	}
	[m_captureSession stopRunning];
}

OP_CAMERA_STATUS CocoaOpCamera::GetCurrentPreviewFrame(OpBitmap** frame)
{
	if (m_capturePreview && [m_capturePreview delegate]) {
		*frame = [[m_capturePreview delegate] getLastFrame];
		return *frame ? OpStatus::OK : OpStatus::ERR;
	}
	return OpCameraStatus::ERR_NOT_AVAILABLE;
}

OP_CAMERA_STATUS CocoaOpCamera::GetPreviewDimensions(Resolution* dimensions)
{
//TODO: Find native resolution, if possible.
	dimensions->x = 640; dimensions->y = 480;
	return OpStatus::OK;
}

#ifdef PI_CAMERA_CAPTURE
OP_CAMERA_STATUS CocoaOpCamera::StartVideoCapture(OpString* file_name, OpCameraCaptureListener* listener, const CaptureParams& params)
{
	StopVideoCapture();
	if (!m_captureFile) {
		BOOL success = NO;
		NSError *error;
		m_captureFile = [[QTCaptureMovieFileOutput alloc] init];
		success = [m_captureSession addOutput:m_captureFile error:&error];
		if (!success)
			return OpStatus::ERR;
	}
	[m_captureFile setDelegate:[[CameraListener alloc] initWithCamera:this capture_listener:listener]];
	[m_captureSession startRunning];
	[m_captureFile recordToOutputFileURL:[NSURL fileURLWithPath:[NSString stringWithCharacters:(const unichar*)file_name->CStr() length:file_name->Length()]]];
	return OpStatus::OK;
}

OP_CAMERA_STATUS CocoaOpCamera::StopVideoCapture()
{
	[m_captureSession stopRunning];
	[m_captureFile recordToOutputFileURL:nil];
	id delegate;
	if ((delegate = [m_captureFile delegate])) {
		[m_captureFile setDelegate:nil];
		[delegate release];
	}
	return OpStatus::OK;
}

OP_CAMERA_STATUS CocoaOpCamera::CaptureImage(OpString* file_name, OpCameraCaptureListener* listener, const CaptureParams& params)
{
	return OpCameraStatus::ERR_NOT_AVAILABLE;
}

OP_CAMERA_STATUS CocoaOpCamera::GetSupportedImageResolutions(OpAutoVector<Resolution>* resolutions)
{
	return OpCameraStatus::ERR_NOT_AVAILABLE;
}

OP_CAMERA_STATUS CocoaOpCamera::GetSupportedVideoResolutions(OpAutoVector<Resolution>* resolutions)
{
	return OpCameraStatus::ERR_NOT_AVAILABLE;
}

OP_CAMERA_STATUS CocoaOpCamera::GetDefaultImageResolution(Resolution* resolution)
{
	NSDictionary* attr = [m_device deviceAttributes];
	NSArray* desc = [m_device formatDescriptions];

	return OpCameraStatus::ERR_NOT_AVAILABLE;
}

OP_CAMERA_STATUS CocoaOpCamera::GetDefaultVideoResolution(Resolution* resolution)
{
	return OpCameraStatus::ERR_NOT_AVAILABLE;
}

#endif // PI_CAMERA_CAPTURE

void CocoaOpCamera::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if (msg == MSG_MAC_CAMERA_UPDATE_FRAME) {
		if (m_preview_listener)
			m_preview_listener->OnCameraUpdateFrame(this);
	}
}

bool CocoaOpCamera::Contains(QTCaptureDevice* device)
{
    if (!device || !m_device)
        return false;
    return [[device uniqueID] compare:[m_device uniqueID]] == NSOrderedSame;
}

void CocoaOpCamera::OnConnectedChange(CocoaOpCameraManager* parent)
{
    bool isConnected = IsConnected();
    bool wasConnected = m_wasConnected;
    m_wasConnected = isConnected;

    if (isConnected != wasConnected)
    {
        if (isConnected)
            parent->CameraAttached(this);
        else
            parent->CameraDetached(this);
    }
    
}

#endif // PI_CAMERA
