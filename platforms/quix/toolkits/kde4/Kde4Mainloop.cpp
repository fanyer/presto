/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "platforms/quix/toolkits/kde4/Kde4Mainloop.h"
#include "platforms/quix/toolkits/ToolkitMainloopRunner.h"
#include <stdio.h>
#include <limits.h>
#include <QTimerEvent>

Kde4Mainloop::Kde4Mainloop(QObject *parent)
  : QObject(parent)
  , m_runner(0)
  , m_timer_id(0)
{
}

void Kde4Mainloop::SetRunner(ToolkitMainloopRunner* runner)
{
	m_runner = runner;
	SetCanCallRunSlice(m_runner != 0);
}

void Kde4Mainloop::SetCanCallRunSlice(bool can_call_run_slice)
{
	if (can_call_run_slice && !m_timer_id)
	{
		// Make it so that KDE will use the main loop runner when running its own loop
		m_timer_id = startTimer(0);
		if (m_timer_id == 0)
			fprintf(stderr, "KDE integration: error starting timer\n");
	}
	else if (!can_call_run_slice && m_timer_id)
	{
		killTimer(m_timer_id);
		m_timer_id = 0;
	}
}

void Kde4Mainloop::timerEvent(QTimerEvent *event)
{
	if (event->timerId() != m_timer_id || !m_runner)
		return;

	unsigned waitms = m_runner->RunSlice();

	if (m_timer_id)
		killTimer(m_timer_id);

	if (waitms != UINT_MAX)
		m_timer_id = startTimer(waitms);
	else
		m_timer_id = 0;
}
