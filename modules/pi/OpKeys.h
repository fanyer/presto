/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef OP_KEYS_H
#define OP_KEYS_H

/** @short Platform-specific key event data.
 *
 * Key events are encoded by the input manager as an input action
 * and passed along for processing inside core. Should that key action
 * be targetted at a platform component (think: plugin), the key event
 * needs to be faithfully reconstructed in the format expected.
 * OpPlatformKeyEventData serves that purpose, holding the required
 * platform-specific details to be able to do so (e.g., exact keysym,
 * scancode etc.). To the key event handling it is just some extra data
 * being passed along.
 *
 * Platforms are responsible for interpreting the contents of this object;
 * to the rest of the system it is merely a blob of data. As the object
 * is kept and propagated within core, methods for supporting that are
 * exposed (but only those.)
 */
class OpPlatformKeyEventData
{
public:
	/** Create a new event data object from the platform-specific event in 'data'.
	 * The object is reference-counted to allow sharing whilst being
	 * propagated through Core. Each site wanting to hold on to a reference
	 * to the object must have balanced calls to IncRef() and DecRef(). (Clearly,
	 * adjusting reference counts is not needed if code merely relays this
	 * object.)
	 *
	 * A freshly constructed instance has a reference count of one (1).
	 * Create()'s caller must not additionally call IncRef(), but is obliged
	 * to call DecRef() when releasing the reference.
	 *
	 * @param[out] key_event_data Upon success, the result object. Will
	 * be NULL if the platform doesn't need to communicate out-of-band
	 * platform event data.
	 * @param data A pointer to a platform event. Owned by the caller,
	 * hence values must be copied out and separately kept in
	 * the event data object being constructed.
	 * @return OpStatus::OK on successful creation, OpStatus::ERR_NO_MEMORY
	 * on OOM.
	 */
	static OP_STATUS Create(OpPlatformKeyEventData **key_event_data, void *data);

	/** Key event data keeps a reference count, @see Create().
	 *
	 * @param event_data The argument can be NULL, but if it
	 * is not then it will have been constructed by Create().
	 */
	static void IncRef(OpPlatformKeyEventData *event_data);
	static void DecRef(OpPlatformKeyEventData *event_data);

	/** Retrieve platform-specific event data needed by Plugin::DeliverKeyEvent()
	 * so that platform key events can be re-created on the plugin wrapper side.
	 *
	 * @param[out] data1 Returns the first part of the platform data
	 * @param[out] data2 Returns the second part of the platform data
	 */
	virtual void GetPluginPlatformData(UINT64& data1, UINT64& data2) = 0;

	virtual ~OpPlatformKeyEventData()
	{
	}
};

#endif // OP_KEYS_H
