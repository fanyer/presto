/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DEP_H
#define DEP_H

#ifndef PROCESS_DEP_ENABLE
#define PROCESS_DEP_ENABLE                          0x00000001
#endif
#ifndef PROCESS_DEP_DISABLE_ATL_THUNK_EMULATION
#define PROCESS_DEP_DISABLE_ATL_THUNK_EMULATION     0x00000002
#endif

enum DEPLevelFlags
{
	MEM_EXECUTE_OPTION_DISABLE = 0x01,					// disable data page execution -> enable DEP
	MEM_EXECUTE_OPTION_ENABLE = 0x02,					// enable data page execution -> disable DEP
	MEM_EXECUTE_OPTION_ATL7_THUNK_EMULATION = 0x04,		// enable ATL thunk emulation
	MEM_EXECUTE_OPTION_PERMANENT = 0x08					// make it permanent so it can't be changed while the process is running
};

void SetEnableDEP();

#endif // DEP_H
