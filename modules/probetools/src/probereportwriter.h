/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2004-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** \author { Anders Oredsson <anderso@opera.com> }
*/

#ifndef _PROBETOOLS_PROBEREPORTWRITER_H
#define _PROBETOOLS_PROBEREPORTWRITER_H

#ifdef SUPPORT_PROBETOOLS

#include "modules/probetools/src/probesnapshot.h"

/*
OpProbereportWriter:

The OpProbereportWriter writes the probedata report, and takes a
snapshot as an input.
*/
class OpProbereportWriter{
private:
	OpFile* opfile;
	URL* report_url;
public:
	OpProbereportWriter();
	OpProbereportWriter(URL& url);
	~OpProbereportWriter();
	OP_STATUS write_report(OpProbeSnapshot* snapshot, double totalSessionTime, const OpStringC& probereport_filename, const OpStringC& probelog_filename);

private:
	OP_STATUS write_report_full(OpProbeSnapshot* snapshot, double totalSessionTime);

	OP_STATUS write_report_part_keynumbers(OpProbeSnapshot* snapshot, double totalSessionTime);
	OP_STATUS write_report_part_probes(OpProbeSnapshotProbe** probe_list, unsigned int probe_count, BOOL extended_info);
	OP_STATUS write_report_part_probesandedges(OpProbeSnapshotProbe** probe_list, unsigned int probe_count);
	OP_STATUS write_report_part_oom(OpProbeSnapshot* snapshot);

	OP_STATUS write_header_line(BOOL extended_info);
	OP_STATUS write_incoming_edge_line(OpProbeEdgeIdentifier& id, OpProbeSnapshotMeasurement* m, int level);
	OP_STATUS write_probe_line(OpProbeIdentifier& id, OpProbeSnapshotMeasurement* m, int level, BOOL extended_info);
	OP_STATUS write_outgoing_edge_line(OpProbeEdgeIdentifier& id, OpProbeSnapshotMeasurement* m, int level);

	OP_STATUS compose_basic_formatted_header(char* target, size_t targetLen);
	OP_STATUS compose_extended_formatted_header(char* target, size_t targetLen);

	OP_STATUS compose_basic_formatted_measurement(char* target, size_t targetLen, int loc, int param, int lev, OpProbeSnapshotMeasurement* m);
	OP_STATUS compose_extended_formatted_measurement(char* target, size_t targetLen, int loc, int param, int lev, OpProbeSnapshotMeasurement* m);

	OP_STATUS write_string(const char* str);

#ifdef PROBETOOLS_LOG_PROBES
	void write_log();
#endif
};

#endif //SUPPORT_PROBETOOLS
#endif //_PROBETOOLS_PROBEREPORTWRITER_H
