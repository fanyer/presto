/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** @author { Morten Rolland <mortenro@opera.com>, Ole Jørgen Anfindsen <olejorgen@opera.com> }
*/

#ifndef _PROBETOOLS_PROBEHELPER_H
#define _PROBETOOLS_PROBEHELPER_H

#ifdef SUPPORT_PROBETOOLS

#include "modules/probetools/generated/probedefines.h"
#include "modules/probetools/src/probetimer.h"
#include "modules/probetools/src/probesnapshot.h"
#include "modules/probetools/src/probegraph.h"
#include "modules/probetools/src/probethreadlocalstorage.h"
#include "modules/windowcommander/OpWindowCommander.h"

#ifdef PROBETOOLS_LOG_PROBES
struct OpProbeInvocation {
	OpProbeTimestamp timestamp;
	const char *name;
	BOOL start; // FALSE if end
};
#endif

/*
OpProbeHelper

OpProbeHelper is the class that porbes access trough g_opera.
Currently it contains a probe_graph, a thread_local_storage, and
a session startup timestamp.

When multithreading is introduced, it will be managed from the
method tls, and sould not affect other parts of probetools.
*/
class OpProbeHelper {
private:

	// Our single large probe graph containing all measured data
	OpProbeGraph *probe_graph;
		
	// Each thread / callstack need one TLS, currently we only support a single one
	OpProbeThreadLocalStorage single_tls;


	OpProbeSnapshot *snapshot;

	// Used to measure the
	OpProbeTimestamp session_start;
	OpProbeTimestamp session_end;

#ifdef PROBETOOLS_LOG_PROBES
	OpProbeInvocation *invocations;
	int used_invocations;
#endif

public:

	// Const / Dest
	OpProbeHelper(void);
	~OpProbeHelper(void);

	// Access
	OpProbeGraph* get_probe_graph();
	double get_session_time();

#ifdef PROBETOOLS_LOG_PROBES
	void add_invocation(const OpProbeTimestamp& timestamp, const char *name, BOOL start);
	void get_invocations(OpProbeInvocation *& i, int &n) { i = invocations; n = used_invocations; }
#endif

	//Thread Local Storage (Future: should give different TLS for different threads)
	OpProbeThreadLocalStorage* tls(void);

	void GeneratePerformancePage(URL& url);
	OpProbeTimestamp &GetSessionStart() { return session_start; }
	void OnStartLoading();
	void OnLoadingFinished();

}; // OpProbeHelper

#endif // SUPPORT_PROBETOOLS
#endif // _PROBETOOLS_PROBEHELPER_H
