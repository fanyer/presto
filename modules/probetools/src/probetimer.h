/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** @author { Morten Rolland <mortenro@opera.com> , Ole Jørgen Anfindsen <olejorgen@opera.com> }
*/

#ifndef _PROBETOOLS_PROBETIMER_H
#define _PROBETOOLS_PROBETIMER_H

#ifdef SUPPORT_PROBETOOLS

/*
OpProbeTimestamp:

The probetools timer wrapper. OpProbeTimestamp encapsulates
timestamp, diff between timestamps, and timestamp calculations.

This makes it possible to keep the implemented timer resolution
until the very end, when get_time() is used to convert the time
into milliseconds.

Currently probetools only implements OP_PROBETOOLS_USES_OP_SYS_INFO_TIMER
which uses "g_op_system_info->GetRuntimeMS()". This timer is wery slow,
and for high-performance timing of low level functionality, add a platform
specific high performance timer with less overhead.

Example implementations is given for OP_PROBETOOLS_USES_WIN32HRPC_TIMER,
and both calculations for time as double, and time as UINT64 is implemented.
*/

//#if defined(WINGOGI) || defined(_WINDOWS) || defined(MSWIN)
	//Windows
	//#define OP_PROBETOOLS_USES_WIN32HRPC_TIMER		
//#elif defined(UNIX) || defined(LINGOGI)
	//Linux
//	#error Not implemented
//	#define OP_PROBETOOLS_USES_GETTIMEOFDAY_TIMER
//#else
	//Fallback to PI (superslow on windows)
	#define OP_PROBETOOLS_USES_OP_SYS_INFO_TIMER
//#endif

class OpProbeTimestamp{
private:
	union{
		double stamp64d;
		UINT64 stamp64i;
	};

public:
	OpProbeTimestamp() : stamp64d(0) {}

	//stamper
	void timestamp_now();
	double get_time();//milliseconds
	void set(double d);//for testing

	//operators
	OpProbeTimestamp operator+(const OpProbeTimestamp& tm);
	OpProbeTimestamp operator-(const OpProbeTimestamp& tm);
	bool operator<(const OpProbeTimestamp& tm);
	bool operator>(const OpProbeTimestamp& tm);
	OpProbeTimestamp operator=(const OpProbeTimestamp& ts);
	OpProbeTimestamp operator+=(const OpProbeTimestamp& tm);
	OpProbeTimestamp operator-=(const OpProbeTimestamp& tm);
	void			 zero();
	
};

#endif //SUPPORT_PROBETOOLS
#endif // _PROBETOOLS_PROBETIMER_H
