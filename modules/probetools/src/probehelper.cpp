/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Implement helper functions declared in probehelper.h
*/

#include "core/pch.h"

#ifdef SUPPORT_PROBETOOLS

#include "modules/probetools/src/probe.h"
#include "modules/probetools/src/probehelper.h"
#include "modules/probetools/src/probesnapshot.h"
#include "modules/probetools/src/probereportwriter.h"
#include "modules/url/url_man.h"
#include "modules/url/protocols/http1.h"
#include "modules/url/protocols/comm.h"

/*==================================================
OpProbeHelper
==================================================*/

OpProbeHelper::OpProbeHelper(void)
:probe_graph(NULL)
,snapshot(NULL)
{
	session_start.timestamp_now();
	single_tls.init();

#ifdef PROBETOOLS_LOG_PROBES
	invocations = OP_NEWA(OpProbeInvocation, PROBE_MAX_STORED_INVOCATIONS);
	used_invocations = 0;
#endif
	probe_graph = OP_NEW(OpProbeGraph,());
}

OpProbeHelper::~OpProbeHelper(void)
{
	//Clear tls
	single_tls.destroy();
	OP_DELETE(probe_graph);
	OP_DELETE(snapshot);
#ifdef PROBETOOLS_LOG_PROBES
	OP_DELETEA(invocations);
#endif
}

OpProbeGraph* OpProbeHelper::get_probe_graph()
{
	return probe_graph;
}

double OpProbeHelper::get_session_time(){
#ifdef OPERA_PERFORMANCE
	if ((urlManager && !urlManager->GetPerformancePageEnabled()) || !session_end.get_time())
#endif // OPERA_PERFORMANCE
		session_end.timestamp_now();
	OpProbeTimestamp session_diff = session_end - session_start;
	return session_diff.get_time();
}

/*
 * Return the "Thread local storage" for current thread.  If UNIX pthreads
 * are not supported, a static data area is returned.
 */
OpProbeThreadLocalStorage* OpProbeHelper::tls(void)
{
	/*
	 * We are not supporting more than a single thread, and we only have
	 * a single static tls_data structure available for use.  This
	 * tls_data structure is initialised by the OpProbeHelper.
	 */
	return &single_tls;
}

void OpProbeHelper::OnStartLoading()
{
	OP_DELETE(probe_graph);
	probe_graph = OP_NEW(OpProbeGraph,());
	session_start.timestamp_now();
}

void OpProbeHelper::OnLoadingFinished()
{
	if (get_probe_graph())
	{
		session_end.timestamp_now();
		OP_DELETE(snapshot);
		snapshot = OpProbeSnapshotBuilder::build_probetools_snapshot(get_probe_graph());
	}
}

void OpProbeHelper::GeneratePerformancePage(URL& url)
{
	if (snapshot)
	{
		OpStringC empty;
		OpProbereportWriter probe_report_writer(url);
		probe_report_writer.write_report(snapshot, get_session_time(), empty, empty);
	}
}

#ifdef PROBETOOLS_LOG_PROBES
void OpProbeHelper::add_invocation(const OpProbeTimestamp& timestamp, const char *name, BOOL start)
{
	if (!invocations || used_invocations >= PROBE_MAX_STORED_INVOCATIONS)
		return;

	invocations[used_invocations].timestamp = timestamp;
	invocations[used_invocations].name = name;
	invocations[used_invocations].start = start;

	used_invocations++;
}
#endif

#endif // SUPPORT_PROBETOOLS
