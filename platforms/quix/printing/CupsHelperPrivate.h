/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */

#ifndef CUPS_HELPER_PRIVATE_H
#define CUPS_HELPER_PRIVATE_H

class DestinationInfo;
class CupsFunctions;

class CupsHelperPrivate
{
public:
	bool GetPaperSizes(DestinationInfo& info, CupsFunctions* functions);
};

#endif
