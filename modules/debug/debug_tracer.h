/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

/** \file
 *
 * \brief Declare an OpTracer object to be used for object/API tracing
 *
 * This file is included automatically as part of the core/pch.h suite
 * of include files.  It makes the \c OpTracer object available for use
 * when API_DBG_OPTRACER is imported.
 *
 * In core-gogi, this API is imported automatically for all debugging
 * builds, so no complete re-compile should be neccesary in order to
 * add a simple OpTracer object for one-off debugging.
 */

#ifndef DEBUG_TRACER_H
#define DEBUG_TRACER_H

#ifdef DEBUG_ENABLE_OPTRACER

/**
 * \brief Enumerator for different types of OpTracer objects
 */
enum OpTracerType
{
	/**
	 * \brief Only track construction and destruction
	 */
	OPTRACER_SINGLE = 0,

	/**
	 * \brief Track simultaneous subscriptions using ref-counts
	 */
	OPTRACER_REFCOUNT = 1,

	/**
	 * \brief Track a value
	 */
	OPTRACER_VALUE = 2
};

class OpTracer
{
public:
	/**
	 * \brief Construct an OpTracer object of a given type
	 *
	 * Initialize an \c OpTracer object.  The \c OpTracer object can be
	 * created in one of several modes.
	 *
	 * Different types of \c OpTracerType:
	 *
	 * <table>
	 * <tr><td>Type</td><td>Usage</td></tr>
	 *
	 * <tr><td>OPTRACER_SINGLE</td>
	 *     <td> The \c OpTracer object is expected to be created once,
	 *          and destroyed, and that's it.  This is the default type,
	 *          and is suited for tracking an object.</td></tr>
	 *
	 * <tr><td>OPTRACER_REFCOUNT</td>
	 *     <td> The \c OpTracer object is created in an object that may be
	 *          used by many others in a "subscription" manner. Each
	 *          subscription can be recorded by a call to the
	 *          \c OpTracer::Subscribe() method, and when the subscription
	 *          comes to an end, the \c OpTracer::Unsubscribe() method
	 *          should be called.</td></tr>
	 *
	 * <tr><td>OPTRACER_VALUE</td>
	 *     <td> The \c OpTracer object is used to record and log values.
	 *          The values can be bytes used, number of images on a page,
	 *          HTTP requests for a given connection or reflow-times.</td></tr>
	 * </table>
	 *
	 * The call-stack of the construction will be logged, so one can determine
	 * what caused the \c OpTracer object to be created.
	 *
	 * \param description A short description of the tracer
	 * \param type The type of tracer to be constructed
	 */
	OpTracer(const char* description, OpTracerType type = OPTRACER_SINGLE);

	/**
	 * \brief Destroy an OpTracer object
	 *
	 * When an \c OpTracer object is destroyed, the call-stack causing this
	 * event to happen will be logged. The final value/ref-count of the
	 * tracer will also be logged.
	 */
	~OpTracer(void);

	/**
	 * \brief Increase the ref-count when subscribing to the OpTracer
	 *
	 * Calling this function on an \c OPTRACER_REFCOUNT type of \c OpTracer
	 * object will cause the ref-count to be increased by one.
	 * The call-stack of the subscribe operation will be logged, along with
	 * the resulting ref-count value.
	 */
	void Subscribe(void);
	void Subscribe(int handle);
	void Subscribe(const void* handle);
	void Subscribe(const char* handle);

	/**
	 * \brief Decrease the ref-count when unsubscribing to the OpTracer
	 *
	 * This is the equivalent to the \c Subscribe() method, only it
	 * decreases the ref-count to signal that some entity is no longer
	 * relying on whatever data/object the \c OpTracer is monitoring.
	 *
	 * The \c Unsubscribe() method should idealy bring the ref-count
	 * down to zero when the object is no longer needed.
	 */
	void Unsubscribe(void);
	void Unsubscribe(int handle);
	void Unsubscribe(const void* handle);
	void Unsubscribe(const char* handle);

	void AddValue(int value);

private:
	void InternalSubscribe(UINT32 cs_id, const char* handle);
	void InternalUnsubscribe(UINT32 cs_id, const char* handle);

	UINT32 tracer_id;
	int value;
	OpTracerType type;
};

#endif // DEBUG_ENABLE_OPTRACER

#endif // DEBUG_TRACER_H
