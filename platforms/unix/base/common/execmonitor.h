// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2008 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Espen Sand
//

#ifndef __EXEC_MONITOR_H__
#define __EXEC_MONITOR_H__

class ExecMonitor
{
public:
	virtual ~ExecMonitor() {}

	static ExecMonitor *get();

	virtual int start(uni_char *program_type, int pref_page) = 0;
};

#endif
