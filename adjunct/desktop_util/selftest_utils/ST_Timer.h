/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef ST_TIMER_H
#define ST_TIMER_H

#ifdef SELFTEST

#include "modules/hardcore/timer/optimer.h"

/** @brief Cause an async test to either pass or fail after x ms
  *
  * Example usage:
  * @code
  * 	global {
  * 		ST_Timer failtimer;
  * 	}
  *
  * 	test("async test") async; {
  * 		failtimer.FailAfter(10000);
  * 	}
  * 	test("cleanup, gets run when async test ends") {
  * 		failtimer.Stop();
  * 	}
  * @endcode
  */
class ST_Timer : private OpTimerListener
{
public:
	ST_Timer()
		{ m_timer.SetTimerListener(this); }

	void PassAfter(unsigned pass_ms)
		{ m_pass_on_time_out = true; m_timer.Start(pass_ms); }

	void FailAfter(unsigned fail_ms)
		{ m_pass_on_time_out = false; m_timer.Start(fail_ms); }

	void Stop()
		{ m_timer.Stop(); }

private:
	virtual void OnTimeOut(OpTimer* timer)
	{
		if (m_pass_on_time_out)
			ST_passed();
		else
			ST_failed("Timed out");
	}

	OpTimer m_timer;
	bool m_pass_on_time_out;
};

#endif // SELFTEST

#endif // ST_TIMER_H
