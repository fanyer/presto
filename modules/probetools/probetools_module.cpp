/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SUPPORT_PROBETOOLS

#include "modules/probetools/probetools_module.h"

#include "modules/probetools/src/probehelper.h"
#include "modules/probetools/src/probesnapshot.h"
#include "modules/probetools/src/probereportwriter.h"
#include "modules/url/url_man.h"

ProbetoolsModule::ProbetoolsModule()
	: m_probehelper(OP_NEW(OpProbeHelper,()))
{
}

void ProbetoolsModule::InitL(const OperaInitInfo& info)
{
	OpProbeTimestamp now;
	now.timestamp_now();
	LEAVE_IF_NULL(m_probehelper);

	OpString tmp;
	tmp.SetL(info.default_folders[OPFILE_HOME_FOLDER]);
	tmp.AppendL("probedata");
	tmp.AppendL(PATHSEP);
# if ENABLED_OP_PROBE_LOG
	int pos = tmp.Length();
	tmp.AppendL("probelog.txt");
	m_probelog_filename.SetL(tmp);
	tmp.Delete(pos); // remove after "probedata/" to append the new name:
# endif //ENABLED_OP_PROBE_LOG
	tmp.AppendL("probereport.txt");
	m_probereport_filename.TakeOver(tmp);
}

void ProbetoolsModule::Destroy()
{
	if (m_probehelper->get_probe_graph())
	{
		//Take probe snapshot and write the report.
		OpProbeSnapshot* probe_report_snapshot = OpProbeSnapshotBuilder::build_probetools_snapshot(m_probehelper->get_probe_graph());
		if(probe_report_snapshot){
			OpProbereportWriter probe_report_writer;
			probe_report_writer.write_report(probe_report_snapshot, m_probehelper->get_session_time(), m_probereport_filename, m_probelog_filename);
			OP_DELETE(probe_report_snapshot);
		}/* else {
			//We got OOM!
		}*/
	}

	OP_DELETE(m_probehelper);
	m_probehelper = 0;
}

#endif // SUPPORT_PROBETOOLS
