/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef KDE4_MAIN_LOOP_H
#define KDE4_MAIN_LOOP_H

#include <QObject>

class ToolkitMainloopRunner;

class Kde4Mainloop : public QObject
{
public:
	Kde4Mainloop(QObject *parent);
	void SetRunner(ToolkitMainloopRunner* runner);
	void SetCanCallRunSlice(bool can_call_run_slice);

private:
	void timerEvent(QTimerEvent *event);

	ToolkitMainloopRunner* m_runner;
	int m_timer_id;
};

#endif // KDE4_MAIN_LOOP_H
