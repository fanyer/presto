/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Kajetan Switalski
**
*/

#ifndef _NEXTPROTOCOL_H_
#define _NEXTPROTOCOL_H_

enum NextProtocol
{
	NP_NOT_NEGOTIATED = 0,
	NP_HTTP,
	NP_SPDY2,
	NP_SPDY3
};

#endif // _NEXTPROTOCOL_H_