/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_DOMSTREAM
#define DOM_DOMSTREAM

#ifdef DOM_STREAM_API_SUPPORT

#include "modules/dom/src/domevents/domeventtarget.h"
#include "modules/dom/src/domruntime.h"
#include "modules/dom/src/domobj.h"
#include "modules/security_manager/include/security_manager.h"
#include "modules/media/camerarecorder.h"
#include "modules/media/mediaelement.h"
#include "modules/media/mediastreamresource.h"
#include "modules/pi/device_api/OpCamera.h"
#include "modules/device_api/OpListenable.h"

class DOM_StreamController;

/** Represents a Stream object obtained from a getUserMedia call.
 *
 * The object may be used to associate a stream (e.g. camera input) to
 * a <video> element.
 */
class DOM_LocalMediaStream : public DOM_Object, public DOM_EventTargetOwner
{
public:
	static OP_STATUS Make(DOM_LocalMediaStream *&stream_object, DOM_Runtime *runtime);

	~DOM_LocalMediaStream();
	OP_STATUS Initialize();

	void Stop();

	OP_STATUS AssociateWithMediaElement(MediaElement *element);

	BOOL IsLive() { return m_is_stream_live; }

	DOM_DECLARE_FUNCTION(createObjectURL);
	enum {
		FUNCTIONS_ARRAY_SIZE = 2,
		FUNCTIONS_WITH_DATA_ARRAY_SIZE = 3
	};

	BOOL IsA(int type) { return type == DOM_TYPE_LOCALMEDIASTREAM || DOM_Object::IsA(type); }

	virtual ES_GetState GetName(const uni_char *property_name, int property_code, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(const uni_char *property_name, int property_code, ES_Value *value, ES_Runtime *origining_runtime);

	virtual void GCTrace();

private:
	DOM_LocalMediaStream() : m_is_stream_live(TRUE), m_stream_controller(NULL) { }

	virtual DOM_Object *GetOwnerObject() { return this; }

	BOOL m_is_stream_live;
	DOM_StreamController *m_stream_controller;
};

/** Associates media elements with DOM_LocalMediaStream and security manager.
 *
 * Security manager may revoke permissions to a certain stream in which case
 * playback needs to be stopped in all asssociated media elements.
 *
 * Objects of this class exist as long as there is at least one media element
 * associated with the stream, even if the DOM_LocalMediaStream object is garbage
 * collected.
 */
class DOM_StreamController
	: public UserConsentListener
	, public MediaStreamResource
	, public OpCameraPreviewListener
	, public CameraRemovedListener
	, public OpListenable<MediaStreamListener>
{
public:
	static OP_STATUS Make(DOM_StreamController*& new_obj, DOM_LocalMediaStream* stream_obj);

	~DOM_StreamController();

	// From UserConsentListener
	virtual void OnUserConsentRevoked();
	virtual void OnBeforeRuntimeDestroyed(DOM_Runtime *runtime);

	// From MediaStreamResource
	virtual OP_STATUS AttachStreamListener(MediaStreamListener* listener);
	virtual void DetachStreamListener(MediaStreamListener* listener);

	virtual const URL &GetOriginURL();

	virtual OP_STATUS GetCurrentFrame(OpBitmap*& frame);
	virtual OP_STATUS GetFrameDimensions(unsigned& width, unsigned& height);

	// From OpCameraPreviewListener
	virtual void OnCameraUpdateFrame(OpCamera* camera);
	virtual void OnCameraFrameResize(OpCamera* camera);

	// From CameraRemovedListener
	virtual void OnCameraRemoved();

private:
	friend class DOM_LocalMediaStream;

	/// defines NotifyUpdateFrame(MediaStreamResource*) method which safely
	/// notifies all MediaStreamListener about a frame update.
	DECLARE_LISTENABLE_EVENT1(MediaStreamListener, UpdateFrame, MediaStreamResource*);

	/// defines NotifyFrameResize(MediaStreamResource*) method which safely
	/// notifies all MediaStreamListener about a frame size change.
	DECLARE_LISTENABLE_EVENT1(MediaStreamListener, FrameResize, MediaStreamResource*);

	/// defines NotifyStreamFinished(MediaStreamResource*) method which safely
	/// notifies all MediaStreamListener about stream being finished.
	DECLARE_LISTENABLE_EVENT1(MediaStreamListener, StreamFinished, MediaStreamResource*);

	DOM_StreamController(const URL &origin_url)
		: m_user_consent_listener_attached(FALSE)
		, m_dom_stream(NULL)
		, m_runtime(NULL)
		, m_origin_url(origin_url)
		, m_blocked(FALSE)
	{}

	OP_STATUS AttachCamera();
	void DetachCamera();

	OP_STATUS AttachUserConsentListener();
	void DetachUserConsentListener();
	BOOL m_user_consent_listener_attached;

	void StreamFinished();
	void DetachListeners();

	static OP_STATUS ConvertCameraStatus(OP_CAMERA_STATUS camera_status);

	void OnStreamObjectDestroyed();
	DOM_LocalMediaStream *m_dom_stream;
	DOM_Runtime *m_runtime;
	URL m_origin_url;
	BOOL m_blocked;
};

#endif // DOM_STREAM_API_SUPPORT

#endif // DOM_DOMSTREAM
