/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef SLEEP_MANAGER_H
#define SLEEP_MANAGER_H

#include "modules/util/adt/oplisteners.h"

#define g_sleep_manager (&SleepManager::GetInstance())

/** @brief Implement this listener if you need to be notified when the system goes in or out of sleep mode
  */
class SleepListener
{
public:
	virtual ~SleepListener() {}

	/** Called when system goes into sleep mode
	  */
	virtual void OnSleep() = 0;

	/** Called when system comes out of sleep mode
	  */
	virtual void OnWakeUp() = 0;
};


/** @brief Listener for the system to see when system needs to stay awake
  */
class StayAwakeListener
{
public:
	virtual ~StayAwakeListener() {}

	/** Called when stay awake status changes
	  * @param stay_awake Whether the system should stay awake
	  */
	virtual void OnStayAwakeStatusChanged(BOOL stay_awake) = 0;
};


/** @brief Interface between the platform and Opera for functions related to sleep/wakeup
  */
class SleepManager
{
public:
	/** Get singleton instance
	  */
	static SleepManager& GetInstance();

	/** Platform should call this function when system goes into sleep mode
	  */
	void	  Sleep();

	/** Platform should call this function when system comes out of sleep mode
	  */
	void	  WakeUp();

	/** Check whether the system is allowed to sleep as far as Opera is concerned
	  * @return Whether Opera allows the system to go to sleep
	  */
	BOOL	  SystemCanGoToSleep();

	/** Start a session in which the system should stay awake
	  * NB: don't forget to call EndStayAwakeSession() when you're done!
	  */
	void	  StartStayAwakeSession();

	/** End a session in which the system should stay awake
	  */
	void	  EndStayAwakeSession();

	/** Add an object that listens to sleep events
	  */
	OP_STATUS AddSleepListener(SleepListener* listener) { return m_listeners.Add(listener); }

	/** Remove an object that listens to sleep events
	  */
	OP_STATUS RemoveSleepListener(SleepListener* listener) { return m_listeners.Remove(listener); }

	/** Set the 'stay awake' listener (should be called by platform code only)
	  */
	void	  SetStayAwakeListener(StayAwakeListener* listener = 0) { m_stay_awake_listener = listener; }

private:
	SleepManager();
	~SleepManager() {}

	OpListeners<SleepListener>	m_listeners;
	StayAwakeListener*			m_stay_awake_listener;
	unsigned					m_stay_awake_sessions;
};


#endif // SLEEP_MANAGER_H
