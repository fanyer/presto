/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef MODULES_HARDCORE_COMPONENT_OPTIMEPROVIDER_H
#define MODULES_HARDCORE_COMPONENT_OPTIMEPROVIDER_H

/** Implementer provides the "current" running time.
  *
  * The "current" running time is defined as time elapsed since some arbitrary event
  * (ex. creating some PI or OpComponent). Not to be confused with wall-clock time
  * or time since starting the application.
  */

class OpTimeProvider
{
public:
	/** Get the time elapsed since an arbitrary event.
	  *
	  * The returned value should be sane, ex. subsequent calls should not
	  * reveal that the run time has decreased.
	  *
	  * @return Number of milliseconds since arbitrary event. Timing precision
	  * is not defined here and may depend on the platform implementation.
	  */
	virtual double GetRuntimeMS() = 0;
};


#endif // MODULES_HARDCORE_COMPONENT_OPTIMEPROVIDER_H
