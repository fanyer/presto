/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef MEDIA_STREAM_RESOURCE_H
#define MEDIA_STREAM_RESOURCE_H

#ifdef MEDIA_CAMERA_SUPPORT

class MediaStreamResource;
class OpBitmap;
class URL;

/** Listener interface for MediaStreamResource. */
class MediaStreamListener
{
public:
	virtual ~MediaStreamListener() {}

	/** Called when a new frame is available. */
	virtual void OnUpdateFrame(MediaStreamResource* stream_resource) = 0;

	/** Called when the dimensions of the frame changes.
	 *
	 * The change may be due to e.g. orientation change for a camera stream. The
	 * method should be called before the frame with new dimensions is
	 * available.
	 */
	virtual void OnFrameResize(MediaStreamResource* stream_resource) = 0;

	/** Called when the stream finishes.
	 *
	 * No other events will be fired after this one and any future calls to
	 * MediaStreamResource::GetCurrentFrame and
	 * MediaStreamResource::GetFrameDimensions will fail.
	 */
	virtual void OnStreamFinished(MediaStreamResource* stream_resource) = 0;
};

/** Represents a stream resource.
 *
 * Note: currently the only stream resource is a camera feed.
 */
class MediaStreamResource
{
public:
	/** Attach a stream listener.
	 *
	 * @return OK if attached successfully.
	 * @return ERR_NOT_AVAILABLE if stream is currently not available, e.g. if
	 *         it is an exclusive resource used by another listener.
	 * @return ERR_NO_MEMORY on OOM.
	 */
	virtual OP_STATUS AttachStreamListener(MediaStreamListener* listener) = 0;

	/** Detach a stream listener. */
	virtual void DetachStreamListener(MediaStreamListener* listener) = 0;

	/** Get the origin URL of the document that created the stream. */
	virtual const URL& GetOriginURL() = 0;

	/** Get the current frame. */
	virtual OP_STATUS GetCurrentFrame(OpBitmap*& frame) = 0;

	/** Get the dimensions of the current frame. */
	virtual OP_STATUS GetFrameDimensions(unsigned& width, unsigned& height) = 0;

	/** Increase the reference count of this object. */
	void IncReferenceCount() { ++m_refcount; OP_ASSERT(m_refcount != 0); }

	/** Decrease the reference count of this object.
	 *
	 *  When the reference count reaches 0 the object will be deleted.
	 *  DO NOT call from within the listener notification!
	 */
	void DecReferenceCount() { OP_ASSERT(m_refcount != 0); if (--m_refcount == 0) OP_DELETE(this); }

protected:
	MediaStreamResource() : m_refcount(0) {}
	virtual ~MediaStreamResource() { OP_ASSERT(m_refcount == 0); }
	unsigned m_refcount;
};

#endif // MEDIA_CAMERA_SUPPORT

#endif // MEDIA_STREAM_RESOURCE_H
