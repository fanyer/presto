/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef PRN_DEV_LISTENERS_H
#define PRN_DEV_LISTENERS_H

#ifdef GENERIC_PRINTING

#include "modules/pi/OpPrinterController.h"

class PrintDevice;

class PrintListener : public OpPrintListener
{
	PrintDevice*	prn_dev;

public:
					PrintListener(PrintDevice* dev);
	void 			OnPrint();
};

#endif // GENERIC_PRINTING
#endif // PRN_DEV_LISTENERS_H
