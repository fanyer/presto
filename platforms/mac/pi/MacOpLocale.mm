/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "platforms/mac/pi/MacOpLocale.h"

#define BOOL NSBOOL
#import <Foundation/NSUserDefaults.h>
#import <Foundation/NSCalendar.h>
#undef BOOL

OP_STATUS MacOpLocale::GetFirstDayOfWeek(int& day)
{
	day = ([[NSCalendar currentCalendar] firstWeekday] - 1) % 7;
	return OpStatus::OK;
}
