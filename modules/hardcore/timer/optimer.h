/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_HARDCORE_TIMER_OPTIMER_H
#define MODULES_HARDCORE_TIMER_OPTIMER_H

#include "modules/hardcore/mh/messobj.h"

class OpTimerListener;

class OpTimer : private MessageObject
{
public:
	OpTimer();
	~OpTimer();

	/** Start the timer
		@param ms The number of milliseconds to timeout
	*/
	void Start(UINT32 ms);

	/** Stop the timer
		@return The number of milliseconds left before timeout
	*/
	UINT32 Stop();

	/** Get the amount of time since firing
		@return The number of milliseconds since firing
		        (a negative value indicates the number of milliseconds left until it fires)
	*/
	int TimeSinceFiring() const;

	/** Returns true if the timer was ever started.
	 *
	 * @note If the timer has already notified the listener, i.e. if
	 *  OpTimerListener::OnTimeOut() was called, then the timer is no
	 *  longer active, but this method still returns true.
	 *
	 * @retval true if Start() was called at least once.
	 * @retval false if Start() was never called.
	 */
	bool IsStarted() const { return !op_isnan(m_firetime); }

	/**
		@param listener The listener that should be notified on time out
	*/
	void SetTimerListener(OpTimerListener* listener) { m_listener = listener; }

private:
	void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	OpTimerListener* m_listener;
	double m_firetime;
};

class OpTimerListener
{
public:
	virtual ~OpTimerListener() {};

	/** Will be called on timeout
		The timer will stop after timeout
		@param timer The timer that has timed out
	*/
	virtual void OnTimeOut(OpTimer* timer) = 0;
};

#endif // !MODULES_HARDCORE_TIMER_OPTIMER_H
