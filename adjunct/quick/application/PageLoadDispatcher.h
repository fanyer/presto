/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef PAGELOADDISPATCHER_H
#define PAGELOADDISPATCHER_H

#include "modules/util/OpSharedPtr.h"
#include "adjunct/quick/windows/DocumentWindowListener.h"
#include "adjunct/quick_toolkit/windows/DesktopWindowListener.h"
#include "modules/util/adt/opvector.h"

class DesktopWindow;
class DocumentDesktopWindow;
class OpSessionWindow;
class OpSession;


/**
 * @brief Object, that can be initialized through PageLoadDispatcher.
 *
 * Purpose of this class is to have an object, that can be initialized
 * in stages, to avoid long initialization in or just after constructor
 * (when it may cause UI visible to user to start or work slowly).
 */

class DispatchableObject
{
public:

	DispatchableObject()
		: m_session_window(NULL)
		, m_has_hidden_internals(false)
	{}

	/**
	 * @brief Return priority of this object.
	 * Based on this priority, PageLoadDispatcher will decide when to initialize it.
	 *
	 * Possible return values are:
	 *  - any negative value - this object should be processed immediately
	 *                      (use this for objects, that don't support
	 *                      dispatch initialization)
	 *  - 0 - init this object ASAP
	 *  - 1..99 - normal priorities
	 *  - 100 or greater - initialize last
	 */
	virtual int GetPriority() const = 0;

	/** 
	 * @brief Properly init object, so it can be shown to user.
	 * When implementing, delay initialization of hidden parts. User may request
	 * those hidden parts to be shown before InitHiddenInternals is called,
	 * therefore such usecase needs to be handled separately.
	 */
	virtual OP_STATUS InitObject() = 0;

	/**
	 * @brief Init only tiniest part of object, so object appears as initialized in UI.
	 * This is used, when we want to show some representation of object to
	 * user, but not object itself.
	 */
	virtual OP_STATUS InitPartial() { return OpStatus::OK; }

	/**
	 * @brief Initialize all hidden parts of object.
	 * No lazy initialization should be needed after this call.
	 */
	virtual OP_STATUS InitHiddenInternals() { return OpStatus::OK; }

	/**
	 * @brief Returns true if there are some hidden internals yet to initialize.
	 */
	virtual bool HasHiddenInternals() { return m_has_hidden_internals; }

	/**
	 * This method needs to be called last in constructor of every implementation of DispatchableOobject.
	 */
	void InitDispatchable();

	/**
	 * Some dispatchable objects need session windows for deserialization purposes.
	 */
	void SetSessionWindow(const OpSessionWindow *session_window) { m_session_window = session_window; }

	const OpSessionWindow* GetSessionWindow() const { return m_session_window; }

protected:

	const OpSessionWindow *m_session_window;
	bool m_has_hidden_internals;
};

class PageLoadDispatcher
	: public DesktopWindowListener
	, public DocumentWindowListener
	, public MessageObject
{
public:

	PageLoadDispatcher()
		: m_queue_closed(true)
		, m_purge_queue(false)
		, m_loading_pages(0)
		, m_unlock_init_hidden_ui(true)
		, m_timestamp(0)
		, m_page_load_limit(0) {}

	~PageLoadDispatcher();

	OP_STATUS Init();

	/**
	 * Set session, that will be loaded.
	 */
	OP_STATUS SetSession(OpSharedPtr<OpSession> session);

	/**
	 * Queue new window for delayed initialization.
	 *
	 * Dispatcher will decide when window should be initialized.
	 */
	OP_STATUS Enqueue(OpSessionWindow *session, DesktopWindow *window);

	/**
	  * Remove object from list of elements for  delayed initialization.
	  */
	OP_STATUS Remove(DispatchableObject *object);

	/**
	 * Delay initialization of hidden UI until at least one page
	 * started loading.
	 */
	void LockInitHiddenUI() { m_unlock_init_hidden_ui = false; }

	/**
	 * Call this once, when you finished queueing session windows.
	 */
	void CloseQueue() { m_queue_closed = true; }

	//
	// DesktopWindowListener
	//

	virtual void OnDesktopWindowActivated(DesktopWindow *desktop_window, BOOL active);

	virtual void OnDesktopWindowClosing(DesktopWindow *desktop_window, BOOL user_initiated);

	//
	// DocumentWindowListener
	//

	virtual void OnStartLoading(DocumentDesktopWindow *document_window);

	virtual void OnLoadingFinished(DocumentDesktopWindow *document_window,
			OpLoadingListener::LoadingFinishStatus,	BOOL was_stopped_by_user);

	//
	// MessageObject
	//

	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

private:

	unsigned FindPosition(int priority) const;
	unsigned FindPosition(int priority, unsigned l, unsigned r) const;

	OP_STATUS Shove();

	OP_STATUS InitHiddenUI();

	/**
	 * Perform first job from load queue.
	 *
	 * @param force Pass false to make sure, that no jobs above allowed limit
	 *              are started; pass true to override.
	 */
	OP_STATUS ProcessQueue(bool force);

	void PurgeQueue();

	OP_STATUS Process(DispatchableObject *window);

	//
	// for use from within listeners:
	//

	OP_STATUS Promote(DesktopWindow *window);

	OpSharedPtr<OpSession> m_session; //< session that is being initialized atm

	OpVector<DispatchableObject> m_load_queue; //< sorted by priority during session creation

	OpVector<DispatchableObject> m_hidden_ui_queue; //< objects with hidden ui to initialize

	OpVector<DispatchableObject> m_prioritized_windows; //< windows, that were pulled from queue out of order

	bool m_queue_closed;

	bool m_purge_queue;

	int m_loading_pages;

	bool m_unlock_init_hidden_ui;

	time_t m_timestamp;

	int m_page_load_limit;
};

#endif // PAGELOADDISPATCHER_H
