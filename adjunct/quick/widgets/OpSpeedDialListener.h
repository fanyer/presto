/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_SPEED_DIAL_LISTENER_H
#define OP_SPEED_DIAL_LISTENER_H

class OpSpeedDialView;

class OpSpeedDialListener
{
	public:
		virtual ~OpSpeedDialListener() {}

		/** Called when speed dial contents change */
		virtual void OnContentChanged(OpSpeedDialView* speeddial) = 0;
};

#endif // OP_SPEED_DIAL_LISTENER_H
