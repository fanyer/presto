/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_IDLE_IDLE_DETECTOR_H
#define MODULES_IDLE_IDLE_DETECTOR_H

#include "modules/hardcore/mh/messobj.h"

/**
 * @file idle_detector.h
 *
 * This file implements "idle detection" in Core.
 *
 * Idle detection attempts to solve the problem of when Core is "sufficiently
 * idle" for a testcase to proceed to the next action. (In particular,
 * testcases often want to know when the consequences of a click on a page are
 * "finished").
 *
 * The idle detection works by placing probe-like objects around in code which
 * performs central tasks, like paint and reflow. These objects report to a
 * manager when their related activity begins and ends, and the manager in
 * turn reports to a set of listeners when we move from an idle state with
 * no live activities, to a busy state (and vice-versa).
 */

// Forward declarations.
class OpActivity;
class OpActivityListener;
class OpIdleDetector;

/**
 * Contains the different states an activity can be in.
 *
 * Not really likely to contain more than two values (ever), but
 * ACTIVITY_STATE_IDLE makes sense faster than a non-descriptive FALSE, when 
 * passed as an parameter for example.
 */
enum OpActivityState
{
	/**
	 * Sent before entering idle state.
	 */
	ACTIVITY_STATE_PRE_IDLE,

	/**
	 * Represents the idle state, i.e. no live activities.
	 */
	ACTIVITY_STATE_IDLE,

	/**
	 * Represents the busy state, i.e. *at least* one live activity.
	 */
	ACTIVITY_STATE_BUSY
};

/**
 * The different types of activity in Core.
 *
 * When activity is reported by OpActivity or OpAutoActivity, the type is
 * carried along with the activation and deactivation events. This enables
 * the OpIdleDetector to understand *why* Opera is not idle.
 */
enum OpActivityType
{
	/**
	 * When the idle detector is disabled (i.e. when there are no listeners),
	 * an activity of this type prevents us from going into the idle state
	 * while no-one is listening.
	 *
	 * This OpActivityType should only be used by OpIdleDetector.
	 */
	ACTIVITY_DISABLED,

	/**
	 * The activity represents the delayed idle check feature of the idle
	 * detector. See OpIdleDetector::DelayedIdleCheck().
	 *
	 * This OpActivityType should only be used by OpIdleDetector.
	 */
	ACTIVITY_DELAYED_IDLE,

	/**
	 * A document is loading, or navigation is taking place.
	 */
	ACTIVITY_DOCMAN,

	/**
	 * Any plugin activity. (This is active as long as a plugin exists).
	 */
	ACTIVITY_PLUGIN,

	/**
	 * A script is running or scheduled.
	 */
	ACTIVITY_ECMASCRIPT,

	/**
	 * A reflow is taking place, or is about to take place.
	 */
	ACTIVITY_REFLOW,

	/**
	 * A paint is taking place.
	 */
	ACTIVITY_PAINT,

	/**
	 * Areas are invalidated.
	 */
	ACTIVITY_INVALIDATION,

	/**
	 * There are animated images.
	 */
	ACTIVITY_IMGANIM,

	/**
	 * A metarefresh is taking place.
	 */
	ACTIVITY_METAREFRESH,

	/**
	 * An SVG is being painted incrementally.
	 */
	ACTIVITY_SVGPAINT,

	/**
	 * An unscripted SVG animation is running.
	 */
	ACTIVITY_SVGANIM,


	/**
	 * Must be last element.
	 */
	ACTIVITY_TYPE_COUNT
};

/**
 * A probe-like object, which acts as the Core-facing API for the idle
 * detector.
 *
 * The class retains some questonable logic from the first version of
 * the idle detector, specifically that it is legal to not balance your
 * OpActivity::Begin/OpActivity::End calls.
 *
 * The object will report to \c g_idle_detector when OpActivity::Begin,
 * OpActivity::End is called, and also (conditionally) when OpActivity::Cancel
 * and OpActivity::~OpActivity is called.
 *
 * OpAutoActivity is a related class which automatically reports activity
 * in the constructor, and automatically reports idle in the destructor.
 *
 * @see OpAutoActivity
 */
class OpActivity
{
public:

	/**
	 * Create a new OpActivity.
	 *
	 * The OpActivity starts in \c ACTIVITY_STATE_IDLE.
	 *
	 * @param type The type of this OpActivity.
	 */
	OpActivity(OpActivityType type);

	/**
	 * Create a new OpActivity which reports to a chosen, non-default
	 * OpIdleDetector.
	 *
	 * The OpActivity starts in \c ACTIVITY_STATE_IDLE.
	 *
	 * @param type The type of this OpActivity.
	 */
	OpActivity(OpActivityType type, OpIdleDetector *detector);

	/**
	 * Destructor. Calls OpActivity::Cancel, which may report to
	 * \c g_idle_detector if 'activity_count' > 0.
	 */
	virtual ~OpActivity();

	/**
	 * Report that this activity is about to begin.
	 *
	 * If the activity can take place more than once at the same time, you may
	 * call this function a corresponding number of times, but this will not
	 * trigger a change in the activity state (because we make a "transition"
	 * from one active state to another active state).
	 */
	void Begin();

	/**
	 * Report that this activity is about to end.
	 *
	 * It is legal to call this more times that OpActivity::Begin has been
	 * called, but it will only trigger an activity state change the first
	 * time the counter hits zero.
	 */
	void End();

	/**
	 * Cancel the activity, and, if currently active, change the activity
	 * state to idle.
	 *
	 * This function makes it possible to set the activity in the idle
	 * state, regardless of the size of the activity counter.
	 */
	void Cancel();

private:

	// Needs access to Detach.
	friend class OpIdleDetector;

	/**
	 * Detach the OpActivity from its OpIdleDetector. This is required to
	 * prevent problems when the ACTIVITY_DELAYED_IDLE OpActivity reports
	 * state changes during the destruction of OpIdleDetector.
	 */
	void Detach() { m_detector = NULL; }

	// The activity counter. This represents how far we are from the idle
	// state (i.e. how may calls to OpActivity::End is needed to put the
	// activity in the idle state).
	int m_activity_count;

	// The type of this activity.
	OpActivityType m_type;

	// The OpIdleDetector to report to.
	OpIdleDetector *m_detector;
};

/**
 * A convenience class for automatically reporting activity.
 *
 * This class will report activity in the constructor, and report idle in the
 * destructor. This is convenient to bind activity to the lifetime of a certain
 * object or function invocation.
 */
class OpAutoActivity
	: public OpActivity
{
public:

	/**
	 * Creates a new OpAutoActivity, and immediately reports activity of the
	 * specified type.
	 *
	 * @param type The type of this activity.
	 */
	OpAutoActivity(OpActivityType type);

	/**
	 * Creates a new OpAutoActivity, and immediately reports activity of the
	 * specified type to the specified OpIdleDetector.
	 *
	 * @param type The type of this activity.
	 */
	OpAutoActivity(OpActivityType type, OpIdleDetector *detector);

	/**
	 * Descrutor. Reports the activity as idle.
	 */
	~OpAutoActivity();
};

/**
 * Implements idle detection.
 *
 * Keeps track of incoming notifications from Op[Auto]Activity objects
 * placed around in the code, and uses this information to figure out
 * whether we are currently idle or busy.
 *
 * When we go from one state to another, we notify listeners of this
 * event.
 *
 * The OpIdleDetector also contains a mechanism for ensuring that
 * we are not "implicitly busy" because of unprocessed messages in the
 * message queue. If we appear to be idle, we issue a message with one
 * millisecond delay, and wait for that message to come through. If we
 * are still idle when it does, we consider this to be a "true idle"
 * state.
 */
class OpIdleDetector
	: public MessageObject
{
public:

	/**
	 * Create a new OpIdleDetector. This is done during hardcore module
	 * initialization, and should happen in no other places. The global
	 * OpIdleDetector singleton can be accessed via \c g_idle_detector.
	 *
	 * Registers this object as a listener for
	 * MSG_IDLE_DELAYED_IDLE_CHECK messages.
	 */
	OpIdleDetector();

	/**
	 * Destructor. Unregisters as a listener for MSG_IDLE_DELAYED_IDLE_CHECK.
	 */
	virtual ~OpIdleDetector();

	/**
	 * Subscribe to activity state changes.
	 *
	 * The OpActivityListener provided will be added to the end of a list
	 * of listeners, and any numbers of listeners may be added.
	 *
	 * The pointer must be valid until RemoveListener is called.
	 *
	 * @param listener The object which will receive events.
	 */
	void AddListener(OpActivityListener *listener);

	/**
	 * Remove a prevously added OpActivityListener.
	 *
	 * If the specified listener is not found, nothing happens.
	 */
	void RemoveListener(OpActivityListener *listener);

	/**
	 * Check whether the idle detector is enabled.
	 *
	 * The idle detector is only enabled if there is at least one listener.
	 *
	 * @return TRUE if enabled, FALSE if not.
	 */
	BOOL IsEnabled() const { return m_active_listeners > 0; }

	/**
	 * Check whether Core is currently idle. This takes into account activity
	 * which may be lined up in the message queue.
	 *
	 * @return TRUE if Core is currently idle, FALSE if not.
	 */
	BOOL IsIdle() const { return m_activity_state == ACTIVITY_STATE_IDLE; }

	/**
	 * This is called with MSG_IDLE_DELAYED_IDLE_CHECK. If we are still idle
	 * when that happens, we notify listeners.
	 */
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

private:

	// Only OpActivity is allowed to report activity to the
	// OpActivityManager.
	friend class OpActivity;

	/**
	 * Called when the idle detector is enabled, i.e. when the first listener
	 * is added.
	 */
	void OnEnable() { m_disabled.End(); }

	/**
	 * Called when the idle detector is disabled, i.e. when the last listener
	 * is removed.
	 */
	void OnDisable() { m_disabled.Begin(); }

	/**
	 * Called from OpActivity and OpAutoActivity when the state of the
	 * activity is changed.
	 *
	 * @param type The type of activity that was changed.
	 * @param state The new state of the activity.
	 */
	void OnActivityStateChanged(OpActivityType type, OpActivityState state);

	/**
	 * Immediately change the current activity state, and broadcasts the new
	 * (global) OpActivityState to listeners while reporting any memory errors
	 * to \c g_memory_manager.
	 *
	 * Global activity state means a state which represents all activites
	 * in Core.
	 *
	 * @param state The new OpActivityState.
	 */
	void ChangeState(OpActivityState state);

	/**
	 * Immediately broadcasts the new (global) OpActivityState to listeners.
	 *
	 * If OpStatus::ERR_NO_MEMORY is returned, it just means we encountered OOM
	 * at some time during iteration. It does not mean that iteration was
	 * cancelled immediately when OOM was encountered. (But there is of course
	 * a reasonably good chance that the listeners could not do what they wanted
	 * because of OOM).
	 *
	 * @param state The new OpActivityState.
	 * @return OpStatus::OK, or OpStatus::ERR_NO_MEMORY.
	 */
	OP_STATUS BroadcastStateChange(OpActivityState state);

	/**
	 * Post a delayed MSG_IDLE_DELAYED_IDLE_CHECK message.
	 *
	 * When we otherwise appear to be idle, we do one final check before
	 * notifying the listeners that the state has changed. We post a
	 * message with a one millisecond delay, and check if we are idle when
	 * that message is handled (by OpIdleDetector::HandleCallback). If we
	 * are still idle, then we know that no work was about to be triggered
	 * by unprocessed messages in the queue.
	 */
	void DelayedIdleCheck();

	// Current activity state.
	OpActivityState m_activity_state;

	// Number of busy activies. If this number is zero (or less), then we are
	// currently idle. We may still not be "truly idle", however, if we more
	// work is lined up in the message queue. See 'DelayedIdleCheck' for more
	// information about that.
	int m_activity_count;

	// First element in list of listeners.
	OpActivityListener *m_first;

	// Last element in list of listeners. New elements are inserted after this
	// element.
	OpActivityListener *m_last;

	// The activity for the delayed idle check. See DelayedIdleCheck() for
	// more information.
	OpActivity m_delayed_idle;

	// While the idle detector is disabled (i.e. when there are no listeners),
	// we will always be BUSY because of this activity. This activity is
	// started when the detector is disabled (or created), and stopped when
	// the detector is enabled. This achieves two things:
	//
	// 1. It prevents us from going into idle before a listener is there to
	//    witness the event. If we are otherwise idle, but this activity
	//    prevents it, we will go into idle as soon as a listener adds
	//    itself.
	// 2. It prevents expensive delayed idle checks when such checks are not
	//    needed.
	OpActivity m_disabled;

	// Number of ACTIVE listeners.
	unsigned int m_active_listeners;
};

/**
 * An interface for listening for changes in the activity state in Core.
 */
class OpActivityListener
{
public:
	/**
	 * Type of activity listener. PASSIVE listeners work as ACTIVE
	 * ones, except that adding or removing a PASSIVE listener will
	 * not affect whether idle detection is enabled or not.
	 */
	enum Type
	{
		/**
		 * Regular listener - idle-detection will be enabled/disabled
		 * as needed when listener is added/removed.
		 */
		ACTIVE,
		/**
		 * Passive listener - adding/removing listener will not affect
		 * whether idle detection is enabled or not.
		 */
		PASSIVE
	};


	/**
	 * Create a new OpActivityListener.
	 * @param type The type of the listener. PASSIVE listeners will
	 * not affect whether idle detection is enabled or not.
	 */
	OpActivityListener(Type type = ACTIVE)
		: m_type(type)
		, m_prev(NULL)
		, m_suc(NULL)
		, m_list(NULL)
	{
	}

	virtual ~OpActivityListener() {}

	/**
	 * Called when a the activity state changes.
	 *
	 * @param state The new activity state.
	 * @return OpStatus::OK, or OpStatus::ERR_NO_MEMORY. All other return
	 *         values are ignored (treated as OpStatus::OK).
	 */
	virtual OP_STATUS OnActivityStateChanged(OpActivityState state) = 0;

private:

	const Type m_type;

	// Give OpIdleDetector access to private variables, so instances can
	// organize listeners in a list.
	friend class OpIdleDetector;

	// Next element in linked list. NULL if this is the first element.
	OpActivityListener *m_prev;

	// Next element in the linked list. NULL if this is the last element.
	OpActivityListener *m_suc;

	// The list this element is inserted in. NULL if the element is not in
	// a list.
	OpIdleDetector *m_list;
};

#endif // MODULES_IDLE_IDLE_DETECTOR_H
