/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2004-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** \author { Anders Oredsson <anderso@opera.com> }
*/

#include "core/pch.h"

#ifdef SUPPORT_PROBETOOLS

#include <stdio.h>

#include "modules/util/opfile/opfile.h"
#include "modules/util/opstring.h"
#include "modules/url/url_man.h"
#include "modules/url/url2.h"
#include "modules/url/protocols/comm.h"
#include "modules/url/protocols/http1.h"
#include "modules/stdlib/include/opera_stdlib.h"

#include "modules/probetools/src/probehelper.h"
#include "modules/probetools/src/probereportwriter.h"
#include "modules/probetools/src/probenamemapper.h"
#include "modules/probetools/generated/probedefines.cpp"

/*==================================================
OpProbereportWriter
==================================================*/
OpProbereportWriter::OpProbereportWriter()
: opfile(0)
, report_url(NULL)
{
}

OpProbereportWriter::OpProbereportWriter(URL& url)
: opfile(0)
, report_url(&url)
{
}

OpProbereportWriter::~OpProbereportWriter()
{
	if (opfile)
	{
		opfile->Close();
		OP_DELETE(opfile);
		opfile = 0;
	}
}

OP_STATUS OpProbereportWriter::write_report(OpProbeSnapshot* snapshot, double totalSessionTime, const OpStringC& probereport_filename, const OpStringC& probelog_filename)
{
	if (!report_url)
	{
		OP_ASSERT(!opfile);
		// open opfile
		OP_STATUS err = 0;
		if ( !(opfile = OP_NEW(OpFile,())) ) { 
			fprintf(stderr, "ERROR: Probetools could not write report because of OOM\n");
			return OpStatus::ERR_NO_MEMORY;
		}
		err = opfile->Construct(probereport_filename, OPFILE_ABSOLUTE_FOLDER); //use file name
		if ( !OpStatus::IsSuccess(err) ) { 
			OP_DELETE(opfile);
			opfile = 0;

			fprintf(stderr, "ERROR: Probetools could not Construct file \"probedata/probereport.txt\"\n");
			fprintf(stderr, "       No probetools report is generated\n");
			fflush(stderr);

			return err;
		}
		err = opfile->Open(OPFILE_WRITE);
		if ( !OpStatus::IsSuccess(err) ) { 
			OP_DELETE(opfile);
			opfile = 0;

			fprintf(stderr, "ERROR: Probetools could not Open file \"probedata/probereport.txt\"\n");
			fprintf(stderr, "       No probetools report is generated\n");
			fflush(stderr);

			return err;
		}
	
		// write report
		OP_STATUS result = write_report_full(snapshot, totalSessionTime);

		// try to add error message to file if failing
		if ( !OpStatus::IsSuccess(result) ) { 
			write_string("ERROR: Error while writing report file\n");

			fprintf(stderr, "ERROR: Probetools could not Write file \"probedata/probereport.txt\", could be OOM\n");
			fprintf(stderr, "       Possibly partial probetools report is generated\n");
			fflush(stderr);
		}

		// close/delete opfile
		opfile->Close();
		OP_DELETE(opfile);
		opfile = 0;

#ifdef PROBETOOLS_LOG_PROBES
		// open opfile
		err = 0;
		if ( !(opfile = OP_NEW(OpFile,())) ) {
			return OpStatus::ERR_NO_MEMORY;
		}

		err = opfile->Construct(probelog_filename, OPFILE_ABSOLUTE_FOLDER);
		if ( !OpStatus::IsSuccess(err) ) {
			OP_DELETE(opfile);
			opfile = 0;
			return err;
		}
		err = opfile->Open(OPFILE_WRITE);
		if ( !OpStatus::IsSuccess(err) ) {
			OP_DELETE(opfile);
			opfile = 0;
			return err;
		}

		// write report
		write_log();

		// close/delete opfile
		opfile->Close();
		OP_DELETE(opfile);
		opfile = 0;
#endif
	
		return result;

	}
	else
	{
		// write report
		return write_report_full(snapshot, totalSessionTime);
	}
}

static int sort_probe_list_comp(const void* ap, const void* bp)
{
	OpProbeSnapshotProbe* a = *((OpProbeSnapshotProbe**)ap);
	OpProbeSnapshotProbe* b = *((OpProbeSnapshotProbe**)bp);
	
	// special case to make root probe go first!
	if(a->get_id().get_location() == OP_PROBE_ROOT) return -1;
	if(b->get_id().get_location() == OP_PROBE_ROOT) return  1;

	// the hight er total time the earlier
	double at = a->get_measurement()->get_total_time();
	double bt = b->get_measurement()->get_total_time();
	if(at > bt) return -1;
	else if(at < bt) return 1;
	else return 0;
}

static int sort_incoming_edge_list_comp(const void* ap, const void* bp)
{
	OpProbeSnapshotEdge* a = *((OpProbeSnapshotEdge**)ap);
	OpProbeSnapshotEdge* b = *((OpProbeSnapshotEdge**)bp);
	double at = a->get_measurement()->get_total_time();
	double bt = b->get_measurement()->get_total_time();
	if(at > bt) return 1;
	else if(at < bt) return -1;
	else return 0;
}

static int sort_outgoing_edge_list_comp(const void* ap, const void* bp)
{
	return sort_incoming_edge_list_comp(ap, bp) * -1;
}

OP_STATUS OpProbereportWriter::write_report_full(OpProbeSnapshot* snapshot, double totalSessionTime)
{
	RETURN_IF_ERROR(write_string("ProbeReportVersion: 1.0\n\n"));

	RETURN_IF_ERROR(write_string("==============\n"
								 "== OVERVIEW ==\n"
								 "==============\n\n"));
	//key numbers
	RETURN_IF_ERROR(write_report_part_keynumbers(snapshot, totalSessionTime));

	RETURN_IF_ERROR(write_string("p->     - Probe\n"));
	RETURN_IF_ERROR(write_string("e       - Edge\n"));
	RETURN_IF_ERROR(write_string("T#      - Total count\nTt      - Total time\nTt(avg) - Average total time\nTt(max) - Max total time\n"));
	RETURN_IF_ERROR(write_string("St      - Self time\nCt      - Children time\nR#      - Recursive count\nRt      - Recursive time\n"));
	RETURN_IF_ERROR(write_string("Ri      - Recursive inited\nO       - Overhead\nO(Avg)  - Average overhead\nLOC     - Location\n"));
	RETURN_IF_ERROR(write_string("PAR     - Index Parameter\nLEV     - Level\n"));
	
	//populate probelist
	unsigned int probe_count = snapshot->get_probe_count();
	if(probe_count <= 0) return OpStatus::OK; //No probes
	OpProbeSnapshotProbe** probe_list = OP_NEWA(OpProbeSnapshotProbe*,probe_count);
	if(!probe_list) return OpStatus::ERR_NO_MEMORY; // OOM
	OpProbeSnapshotProbe* probe = snapshot->get_first_probe();
	for(unsigned int i=0;i<probe_count;i++)
	{
		probe_list[i] = probe;
		probe = probe->get_next();
	}

	// sort probelist
	op_qsort(probe_list, probe_count, sizeof(OpProbeSnapshotProbe*), sort_probe_list_comp);

	OP_STATUS result;


	//BASIC PROBE INFO
	RETURN_IF_ERROR(write_string("\n\n======================\n"
								     "== BASIC PROBE INFO ==\n"
								     "======================\n\n"));
	result = write_report_part_probes(probe_list, probe_count, FALSE);
	if(!OpStatus::IsSuccess(result)){
		OP_DELETEA(probe_list);
		return result;
	}

	//EXTENDED PROBE INFO
	RETURN_IF_ERROR(write_string("\n\n=========================\n"
								     "== EXTENDED PROBE INFO ==\n"
								     "=========================\n\n"));
	result = write_report_part_probes(probe_list, probe_count, TRUE);
	if(!OpStatus::IsSuccess(result)){
		OP_DELETEA(probe_list);
		return result;
	}

	//PROBES AND EDGES
	RETURN_IF_ERROR(write_string("\n\n======================\n"
						 		     "== PROBES AND EDGES ==\n"
								     "======================\n\n"));
	result = write_report_part_probesandedges(probe_list, probe_count);
	if(!OpStatus::IsSuccess(result)){
		OP_DELETEA(probe_list);
		return result;
	}

	// delete probe_list
	OP_DELETEA(probe_list);
	
	RETURN_IF_ERROR(write_string("\n\n"
                                 "=========\n"
								 "== OOM ==\n"
								 "=========\n\n"));
	RETURN_IF_ERROR(write_report_part_oom(snapshot));

	return OpStatus::OK;
}

OP_STATUS OpProbereportWriter::write_report_part_keynumbers(OpProbeSnapshot* snapshot, double totalSessionTime)
{
	// key numbers
	OpProbeSnapshotProbe* probe = snapshot->get_first_probe();
	double totalSelf = 0;
	double totalOverhead = 0;
	
	if(!probe) return OpStatus::OK; //No probes

	while (probe)
	{
		totalSelf += probe->get_measurement()->get_self_time();
		totalOverhead += probe->get_measurement()->get_overhead_time();
		probe = probe->get_next();
	}

	char line[256]; /* ARRAY OK 2010-04-15 anderso */
	double idleTime = totalSessionTime - (totalSelf + totalOverhead);

	op_snprintf(line, sizeof(line), 
		"Session length (incl idle): %.3f\n"
		"Probe time (w/o oh): %.3f\n"
		"Probe overhead: %.3f\n"
		"Probe overhead%%: %.1f%%\n"
		"Idle (outside probes): %.3f\n"
		"Idle percentage: %.1f%%\n"
		"Runtime overhead%%: %.1f%%\n"
		"\n",
		totalSessionTime,
		totalSelf,
		totalOverhead,
		(totalOverhead/(totalSelf+totalOverhead))*100.0,
		idleTime,
		(idleTime/totalSessionTime)*100.0,
		(totalOverhead/totalSessionTime)*100.0
	);
	RETURN_IF_ERROR(write_string(line));
	
	return OpStatus::OK;
}

OP_STATUS OpProbereportWriter::write_report_part_probes(OpProbeSnapshotProbe** probe_list, unsigned int probe_count, BOOL extended_info)
{

	
	// header
	RETURN_IF_ERROR(write_header_line(extended_info));
	
	for(unsigned int i=0;i<probe_count;i++)
	{
		OpProbeSnapshotProbe* probe = probe_list[i];

		// print probe
		RETURN_IF_ERROR(write_probe_line(
			probe->get_id(),
			probe->get_measurement(),
			probe->get_level(), 
			extended_info
		));
	}

	RETURN_IF_ERROR(write_string("\n"));

	return OpStatus::OK;
}

OP_STATUS OpProbereportWriter::write_report_part_probesandedges(OpProbeSnapshotProbe** probe_list, unsigned int probe_count)
{

	// iter probelist
	for(unsigned int i=0;i<probe_count;i++)
	{
		OpProbeSnapshotProbe* probe = probe_list[i];
		
		// header
		RETURN_IF_ERROR(write_header_line(TRUE));

		// incoming
		unsigned int incoming_count = probe->get_incoming_edge_count();
		if(incoming_count > 0)
		{
			unsigned int j;

			// populate incoming list
			OpProbeSnapshotEdge** incoming_list = OP_NEWA(OpProbeSnapshotEdge*,incoming_count);
			if(!incoming_list){
				return OpStatus::ERR_NO_MEMORY;
			}
			OpProbeSnapshotEdge* incoming = probe->get_first_incoming_edge();
			for(j=0;j<incoming_count;j++)
			{
				incoming_list[j] = incoming;
				incoming = incoming->get_next();
			}

			// sort list
			op_qsort(incoming_list, incoming_count, sizeof(OpProbeSnapshotEdge*), sort_incoming_edge_list_comp);

			// print list
			for(j=0;j<incoming_count;j++)
			{
				incoming = incoming_list[j];
				OP_STATUS result = write_incoming_edge_line(
					incoming->get_id(),
					incoming->get_measurement(),
					incoming->get_level()
				);
				if(!OpStatus::IsSuccess(result)){
					OP_DELETEA(incoming_list);
					return result;
				}
			}

			// delete list
			OP_DELETEA(incoming_list);
		}

		// print probe
		{
			RETURN_IF_ERROR(write_probe_line(
				probe->get_id(),
				probe->get_measurement(),
				probe->get_level(), 
				TRUE
			));
		}

		// outgoing
		unsigned int outgoing_count = probe->get_outgoing_edge_count();
		if(outgoing_count > 0)
		{
			unsigned int j;

			// populate outgoing list
			OpProbeSnapshotEdge** outgoing_list = OP_NEWA(OpProbeSnapshotEdge*,outgoing_count);
			if(!outgoing_list){
				return OpStatus::ERR_NO_MEMORY;
			}
			OpProbeSnapshotEdge* outgoing = probe->get_first_outgoing_edge();
			for(j=0;j<outgoing_count;j++)
			{
				outgoing_list[j] = outgoing;
				outgoing = outgoing->get_next();
			}
			
			// sort list
			op_qsort(outgoing_list, outgoing_count, sizeof(OpProbeSnapshotEdge*), sort_outgoing_edge_list_comp);

			// print list
			for(j=0;j<outgoing_count;j++)
			{
				outgoing = outgoing_list[j];
				OP_STATUS result = write_outgoing_edge_line(
					outgoing->get_id(),
					outgoing->get_measurement(),
					outgoing->get_level()
				);
				if(!OpStatus::IsSuccess(result)){
					OP_DELETEA(outgoing_list);
					return result;
				}
			}
			
			// delete list
			OP_DELETEA(outgoing_list);
		}

		RETURN_IF_ERROR(write_string("\n"));
	}

	return OpStatus::OK;
}

OP_STATUS OpProbereportWriter::write_report_part_oom(OpProbeSnapshot* snapshot)
{
	RETURN_IF_ERROR(write_header_line(TRUE));
	char composedMeasurement[256]; /* ARRAY OK 2010-04-15 anderso */
	RETURN_IF_ERROR(compose_extended_formatted_measurement(composedMeasurement, sizeof(composedMeasurement), 0, 0, 0, snapshot->get_oom_measurement()));

	char line[256]; /* ARRAY OK 2010-04-15 anderso */
	op_snprintf(line, sizeof(line),"oom %s OOM\n\n",composedMeasurement);
	RETURN_IF_ERROR(write_string(line));

	return OpStatus::OK;
}

OP_STATUS OpProbereportWriter::write_header_line(BOOL extended_info)
{
	char composedHeader[256]; /* ARRAY OK 2010-04-15 anderso */
	if (!extended_info)
		RETURN_IF_ERROR(compose_basic_formatted_header(composedHeader, sizeof(composedHeader)));
	else
		RETURN_IF_ERROR(compose_extended_formatted_header(composedHeader, sizeof(composedHeader)));


	char line[256]; /* ARRAY OK 2010-04-15 anderso */
	op_snprintf(line, sizeof(line), "--- %s NAME\n", composedHeader);
	RETURN_IF_ERROR(write_string(line));

	return OpStatus::OK;
}

OP_STATUS OpProbereportWriter::write_incoming_edge_line(OpProbeEdgeIdentifier& id, OpProbeSnapshotMeasurement* m, int level)
{
	char composedMeasurement[256]; /* ARRAY OK 2010-04-15 anderso */
	RETURN_IF_ERROR(compose_extended_formatted_measurement(composedMeasurement, sizeof(composedMeasurement), id.get_parent_location(), id.get_parent_index_parameter(), level, m));

	char name[64]; /* ARRAY OK 2010-04-15 anderso */
	OpProbeIdentifier parent_id = id.get_parent_id();
	if(!OpProbeNameMapper::GetMappedProbeParameterName(parent_id,name,sizeof(name))){
		//strcpy as it should have been!
		op_memcpy(name, m->get_name(), MIN(sizeof(name)-1,op_strlen(m->get_name())+1));
		name[sizeof(name)-1] = 0;
	}

	char line[256]; /* ARRAY OK 2010-04-15 anderso */
	op_snprintf(line, sizeof(line),
		"e   %s %s %s\n",
		composedMeasurement,
		name,
		probedefines_get_probemodule(id.get_parent_location())
	);
	RETURN_IF_ERROR(write_string(line));

	return OpStatus::OK;
}

OP_STATUS OpProbereportWriter::write_probe_line(OpProbeIdentifier& id, OpProbeSnapshotMeasurement* m, int level, BOOL extended_info)
{
	char composedMeasurement[256]; /* ARRAY OK 2010-04-15 anderso */
	if (!extended_info)
		RETURN_IF_ERROR(compose_basic_formatted_measurement(composedMeasurement, sizeof(composedMeasurement), id.get_location(), id.get_index_parameter(), level, m));
	else
		RETURN_IF_ERROR(compose_extended_formatted_measurement(composedMeasurement, sizeof(composedMeasurement), id.get_location(), id.get_index_parameter(), level, m));
	
	char name[64]; /* ARRAY OK 2010-04-15 anderso */
	if(!OpProbeNameMapper::GetMappedProbeParameterName(id,name,sizeof(name))){
		//strcpy as it should have been!
		op_memcpy(name, m->get_name(), MIN(sizeof(name)-1,op_strlen(m->get_name())+1));
		name[sizeof(name)-1] = 0;
	}
	
	char line[256]; /* ARRAY OK 2010-04-15 anderso */
	op_snprintf(line, sizeof(line),
		"p-> %s %s %s\n",
		composedMeasurement,
		name,
		probedefines_get_probemodule(id.get_location())
	);
	RETURN_IF_ERROR(write_string(line));

	return OpStatus::OK;
}

OP_STATUS OpProbereportWriter::write_outgoing_edge_line(OpProbeEdgeIdentifier& id, OpProbeSnapshotMeasurement* m, int level)
{
	char composedMeasurement[256]; /* ARRAY OK 2010-04-15 anderso */
	RETURN_IF_ERROR(compose_extended_formatted_measurement(composedMeasurement, sizeof(composedMeasurement), id.get_child_location(), id.get_child_index_parameter(), level, m));

	char name[64]; /* ARRAY OK 2010-04-15 anderso */
	OpProbeIdentifier child_id = id.get_child_id();
	if(!OpProbeNameMapper::GetMappedProbeParameterName(child_id,name,sizeof(name))){
		//strcpy as it should have been!
		op_memcpy(name, m->get_name(), MIN(sizeof(name)-1,op_strlen(m->get_name())+1));
		name[sizeof(name)-1] = 0;
	}

	char line[256]; /* ARRAY OK 2010-04-15 anderso */
	op_snprintf(line, sizeof(line),
		"e   %s %s %s\n",
		composedMeasurement,
		name,
		probedefines_get_probemodule(id.get_child_location())
	);
	RETURN_IF_ERROR(write_string(line));

	return OpStatus::OK;
}

OP_STATUS OpProbereportWriter::compose_basic_formatted_header(char* target, size_t targetLen)
{
	const char header[] =
		//HEADER
		//total_count : 8
		"------T#"		" "
		//total_time : 12
		"----------Tt"	" "
		//avg_total_time : 11
		"----Tt(avg)"	" "
		//max_total_time : 11
		"----Tt(max)"	" "
		//self_time : 11
		"---------St"	" ";

	//strcpy as it should have been!
	op_memcpy(target, header, MIN(targetLen-1,op_strlen(header)+1));
	target[targetLen-1] = 0;
	
	return OpStatus::OK;
}

OP_STATUS OpProbereportWriter::compose_extended_formatted_header(char* target, size_t targetLen)
{
	const char header[] =
		//HEADER
		//total_count : 8
		"------T#"		" "
		//total_time : 12
		"----------Tt"	" "
			//avg_total_time : 11
			"----Tt(avg)"	" "
			//max_total_time : 11
			"----Tt(max)"	" "
		//self_time : 11
		"---------St"	" "
		//children_time : 11
		"---------Ct"	" "
		//recursive count : 8
		"------R#"		" "
		//recursive time : 11
		"---------Rt"	" "
		//recursive inited time : 11
		"---------Ri"	" "
		//overhead : 11
		"----------O"		" "
			//avg_overhead : 11
			"-----O(avg)"		" "
			//loc : 4
			"-LOC"				" "
			//param : 4
			"-PAR"				" "
			//level : 4
			"-LEV";

	//strcpy as it should have been!
	op_memcpy(target, header, MIN(targetLen-1,op_strlen(header)+1));
	target[targetLen-1] = 0;
	
	return OpStatus::OK;
}


OP_STATUS OpProbereportWriter::compose_basic_formatted_measurement(char* target, size_t targetLen, int loc, int param, int lev, OpProbeSnapshotMeasurement* m)
{	
	op_snprintf(
		target,
		targetLen,

		//FORMAT
		//total_count : 8
		"%8i"		" "
		//total_time : 12
		"%12.3f"	" "
		//avg_total_time : 11
		"%11.5f"	" "
		//max_total_time : 11
		"%11.5f"	" "
		//self_time : 11
		"%11.3f"	" "
		,

		//DATA
		//total_count : 8
		m->get_total_count(),
		//total_time : 12
		m->get_total_time(),
		//avg_total_time : 11
		(m->get_total_count()>0 ? m->get_total_time()/m->get_total_count() : 0),
		//max_total_time : 11
		m->get_max_total_time(),
		//self_time : 11
		m->get_self_time()
	);

	return OpStatus::OK;
}


OP_STATUS OpProbereportWriter::compose_extended_formatted_measurement(char* target, size_t targetLen, int loc, int param, int lev, OpProbeSnapshotMeasurement* m)
{
	op_snprintf(
		target,
		targetLen,

		//FORMAT
		//total_count : 8
		"%8i"		" "
		//total_time : 12
		"%12.3f"	" "
			//avg_total_time : 11
			"%11.5f"	" "
			//max_total_time : 11
			"%11.5f"	" "
		//self_time : 11
		"%11.3f"	" "
		//children_time : 11
		"%11.3f"	" "
		//recursive count : 8
		"%8i"		" "
		//recursive time : 11
		"%11.3f"	" "
		//recursive inited time : 11
		"%11.3f"	" "
		//overhead :11
		"%11.3f"		" "
			//avg_overhead : 11
			"%11.5f"		" "
			//loc : 4
			"%4i"			" "
			//param : 4
			"%4i"			" "
			//level : 4
			"%4i"
		,

		//DATA
		//total_count : 8
		m->get_total_count(),
		//total_time : 12
		m->get_total_time(),
			//avg_total_time : 11
			(m->get_total_count()>0 ? m->get_total_time()/m->get_total_count() : 0),
			//max_total_time : 11
			m->get_max_total_time(),
		//self_time : 11
		m->get_self_time(),
		//children_time : 11
		m->get_children_time(),
		//recursive count : 8
		m->get_recursive_count(),
		//recursive time : 11
		m->get_recursive_self_time(),
		//recursive inited time : 11
		m->get_recursion_initiated_time(),
		//overhead 11
		m->get_overhead_time(),
			//avg_overhead : 11
			(m->get_total_count()>0 ? m->get_overhead_time()/m->get_total_count() : 0),
			//loc
			loc,
			//param
			param,
			//level
			lev
	);

	return OpStatus::OK;
}

OP_STATUS OpProbereportWriter::write_string(const char* str)
{
	if (opfile)
		return opfile->Write(str,op_strlen(str));
	else if (report_url)
	{
		OpString text;
		text.Set(str);
		return report_url->WriteDocumentDataUniSprintf(UNI_L("<pre>%s</pre>"), text.CStr());
	}
	else
		return OpStatus::ERR;
}

#ifdef PROBETOOLS_LOG_PROBES

void OpProbereportWriter::write_log()
{
	OpProbeHelper* probehelper = g_probetools_helper;
	OpString8 str;
	OpProbeInvocation *invocations;
	int n_invocations;
	probehelper->get_invocations(invocations, n_invocations);

	for (int i=0; i<n_invocations; i++)
	{
		str.Empty();
		str.AppendFormat("%f: %s (%s)\n", invocations[i].timestamp.get_time() / 1000.0, invocations[i].name, invocations[i].start ? "BEGIN" : "END");
		write_string(str.CStr());
	}
}

#endif // PROBETOOLS_LOG_PROBES

#endif
