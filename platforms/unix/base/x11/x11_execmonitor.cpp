// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2008 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Espen Sand
//

#include "core/pch.h"

#include "platforms/unix/base/x11/x11_execmonitor.h"

// static
ExecMonitor *ExecMonitor::get()
{
    static X11ExecMonitor monitor;
    return &monitor;
}

int X11ExecMonitor::start(uni_char *program_type, int pref_page)
{
#warning "Implement X11ExecMonitor::start"
    return 0;
}
