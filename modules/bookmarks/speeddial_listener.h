/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2008 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef OP_SPEEDDIAL_LISTENER_H
#define OP_SPEEDDIAL_LISTENER_H

#ifdef CORE_SPEED_DIAL_SUPPORT

class SpeedDial;

class OpSpeedDialListener
{
public:
	virtual ~OpSpeedDialListener() { }

	/**
	 * Called when a speed dial has been added or modified.
	 */
	virtual OP_STATUS OnSpeedDialChanged(SpeedDial* speed_dial) = 0;

	/**
	 * Called when a speed dial has been modified.
	 */
	virtual OP_STATUS OnSpeedDialRemoved(SpeedDial* speed_dial) = 0;
};

#endif // CORE_SPEED_DIAL_SUPPORT

#endif // OP_SPEED_DIAL_LISTENER
