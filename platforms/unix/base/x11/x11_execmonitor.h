// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2008 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Espen Sand
//

#ifndef __X11_EXEC_MONITOR_H__
#define __X11_EXEC_MONITOR_H__

#include "platforms/unix/base/common/execmonitor.h"

class X11ExecMonitor :
	public ExecMonitor
{
public:
	virtual ~X11ExecMonitor() {}

	virtual int start(uni_char *program_type, int pref_page);
};

#endif // __X11_EXEC_MONITOR_H__
