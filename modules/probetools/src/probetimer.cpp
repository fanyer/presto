/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** This file needs to exist since inlining 'OpProbeTime::TrueSeconds' would
** cause cyclic include file problems.
**
** Morten Rolland, mortenro@opera.com
*/

#include "core/pch.h"

#ifdef SUPPORT_PROBETOOLS

#include "modules/probetools/src/probetimer.h"
#include "modules/pi/OpSystemInfo.h"

//---------------------------------------
//STAMPER FOR OP_SYS_INFO->GetRuntimeMS==
#if defined(OP_PROBETOOLS_USES_OP_SYS_INFO_TIMER)
	
	/*inline*/
	void OpProbeTimestamp::timestamp_now(){
		if(g_opera && g_op_system_info){//needed for OpFile probes under startup
			stamp64d = g_op_time_info->GetRuntimeMS();
		} else {
			stamp64d = 0;
		}
	}

	double OpProbeTimestamp::get_time(){
		return stamp64d;
	}

	void OpProbeTimestamp::set(double d){
		stamp64d = d;
	}

//---------------------------------------
//STAMPER FOR WIN32HRPC
#elif defined(OP_PROBETOOLS_USES_WIN32HRPC_TIMER)

#include <sys/timeb.h>

	/*inline*/
	void OpProbeTimestamp::timestamp_now(){
		LARGE_INTEGER now_qpc;
		QueryPerformanceCounter(&now_qpc);
		stamp64i = now_qpc.QuadPart;
	}

	double OpProbeTimestamp::get_time(){
		LARGE_INTEGER qpc_freq;
		QueryPerformanceFrequency(&qpc_freq);
		return (((stamp64i)/static_cast<double>(qpc_freq.QuadPart))*1000.0);
	}

	void OpProbeTimestamp::set(double d){
		LARGE_INTEGER qpc_freq;
		QueryPerformanceFrequency(&qpc_freq);
		stamp64i = (((d)*static_cast<double>(qpc_freq.QuadPart))/1000.0);
	}

#endif
//DONE WITh STAMPERS
//---------------------------------------

//---------------------------------------
//OPERATORS FOR (stamp64d)==
#if defined(OP_PROBETOOLS_USES_OP_SYS_INFO_TIMER)

	OpProbeTimestamp OpProbeTimestamp::operator+(const OpProbeTimestamp& tm)
	{
		OpProbeTimestamp sum;
		sum.stamp64d = this->stamp64d + tm.stamp64d;
		return sum;
	}
	OpProbeTimestamp OpProbeTimestamp::operator-(const OpProbeTimestamp& tm)
	{
		OpProbeTimestamp sum;
		sum.stamp64d = this->stamp64d - tm.stamp64d;
		return sum;
	}
	bool OpProbeTimestamp::operator<(const OpProbeTimestamp& tm)
	{
		return this->stamp64d < tm.stamp64d;
	}
	bool OpProbeTimestamp::operator>(const OpProbeTimestamp& tm)
	{
		return this->stamp64d > tm.stamp64d;
	}
	OpProbeTimestamp OpProbeTimestamp::operator=(const OpProbeTimestamp& ts)
	{
		this->stamp64d = ts.stamp64d;
		return *this;
	}
	OpProbeTimestamp OpProbeTimestamp::operator+=(const OpProbeTimestamp& tm)
	{
		this->stamp64d += tm.stamp64d;
		return *this;
	}
	OpProbeTimestamp OpProbeTimestamp::operator-=(const OpProbeTimestamp& tm)
	{
		this->stamp64d -= tm.stamp64d;
		return *this;
	}
	void OpProbeTimestamp::zero(){
		stamp64d = 0.0;
	}

//---------------------------------------
//OPERATORS FOR (stamp64i)==
#elif defined(OP_PROBETOOLS_USES_WIN32HRPC_TIMER)	

	OpProbeTimestamp OpProbeTimestamp::operator+(const OpProbeTimestamp& tm)
	{
		OpProbeTimestamp sum;
		sum.stamp64i = this->stamp64i + tm.stamp64i;
		return sum;
	}
	OpProbeTimestamp OpProbeTimestamp::operator-(const OpProbeTimestamp& tm)
	{
		OpProbeTimestamp sum;
		sum.stamp64i = this->stamp64i - tm.stamp64i;
		return sum;
	}
	bool OpProbeTimestamp::operator<(const OpProbeTimestamp& tm)
	{
		return this->stamp64i < tm.stamp64i;
	}
	bool OpProbeTimestamp::operator>(const OpProbeTimestamp& tm)
	{
		return this->stamp64i > tm.stamp64i;
	}
	OpProbeTimestamp OpProbeTimestamp::operator=(const OpProbeTimestamp& ts)
	{
		this->stamp64i = ts.stamp64i;
		return *this;
	}
	OpProbeTimestamp OpProbeTimestamp::operator+=(const OpProbeTimestamp& tm)
	{
		this->stamp64i += tm.stamp64i;
		return *this;
	}
	OpProbeTimestamp OpProbeTimestamp::operator-=(const OpProbeTimestamp& tm)
	{
		this->stamp64i -= tm.stamp64i;
		return *this;
	}
	void OpProbeTimestamp::zero(){
		stamp64i = 0;
	}

#endif
//DONE WITh OPERATORS
//---------------------------------------

#endif //SUPPORT_PROBETOOLS
