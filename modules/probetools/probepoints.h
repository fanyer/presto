/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/*
 * Include modules/probetools/probepoints.h wherever in opera you want to place a probe!
 */

#ifndef _PROBETOOLS_PROBEPOINTS_H
#define _PROBETOOLS_PROBEPOINTS_H

#include "modules/probetools/generated/probedefines.h"

#ifdef SUPPORT_PROBETOOLS

#include "modules/probetools/probepoint_enabling.h"
#include "modules/probetools/src/probe.h"

//---------------------------------------------------------
//MAIN PROBE DEFINITIONS
#define OP_PROBE_INTERNAL_SETUP(level, location, location_name) \
	OpProbeTimestamp preamble_start_ts; \
	preamble_start_ts.timestamp_now(); \
	OpProbe _op_probe(level, location, location_name, preamble_start_ts)

#define OP_PROBE_INTERNAL_SETUP_WITH_PARAM(level, location, location_name, location_param) \
	OpProbeTimestamp preamble_start_ts; \
	preamble_start_ts.timestamp_now(); \
	OpProbe _op_probe(level, location, location_name, preamble_start_ts); \
	_op_probe.set_probe_index_parameter(location_param)

#define OP_PROBE_INTERNAL_CLOSE() \
	_op_probe.preamble_stop_ts.timestamp_now()

#define OP_PROBE_INTERNAL(level, location, location_name) \
	OP_PROBE_INTERNAL_SETUP(level, location, location_name); \
	OP_PROBE_INTERNAL_CLOSE()

#define OP_PROBE_INTERNAL_WITH_PARAM(level, location, location_name, location_param) \
	OP_PROBE_INTERNAL_SETUP_WITH_PARAM(level, location, location_name, location_param); \
	OP_PROBE_INTERNAL_CLOSE()

#define OP_PROBE_INTERNAL_L(level, location, location_name) \
	OP_PROBE_INTERNAL_SETUP(level, location, location_name); \
	ANCHOR(OpProbe, _op_probe); \
	OP_PROBE_INTERNAL_CLOSE()

#define OP_PROBE_INTERNAL_WITH_PARAM_L(level, location, location_name, location_param) \
	OP_PROBE_INTERNAL_SETUP_WITH_PARAM(level, location, location_name, location_param); \
	ANCHOR(OpProbe, _op_probe); \
	OP_PROBE_INTERNAL_CLOSE()

/*
 * Only if the respective probes are enabled should the macros be expanded.
 */

#if !defined( OP_PROBE0 ) && ( ENABLED_OP_PROBE0 )
# define OP_PROBE0(loc) OP_PROBE_INTERNAL(0, loc, #loc)
# define OP_PROBE0_PARAM(loc,par) OP_PROBE_INTERNAL_WITH_PARAM(0, loc, #loc, par)
#elif !defined( OP_PROBE0 )
# define OP_PROBE0(loc)
# define OP_PROBE0_PARAM(loc,par)
#endif

#if !defined( OP_PROBE1 ) && ( ENABLED_OP_PROBE1 )
# define OP_PROBE1(loc) OP_PROBE_INTERNAL(1, loc, #loc)
# define OP_PROBE1_PARAM(loc,par) OP_PROBE_INTERNAL_WITH_PARAM(0, loc, #loc, par)
#elif !defined( OP_PROBE1 )
# define OP_PROBE1(loc)
# define OP_PROBE1_PARAM(loc,par)
#endif

#if !defined( OP_PROBE2 ) && ( ENABLED_OP_PROBE2 )
# define OP_PROBE2(loc) OP_PROBE_INTERNAL(2, loc, #loc)
# define OP_PROBE2_PARAM(loc,par) OP_PROBE_INTERNAL_WITH_PARAM(0, loc, #loc, par)
#elif !defined( OP_PROBE2 )
# define OP_PROBE2(loc)
# define OP_PROBE2_PARAM(loc,par)
#endif

#if !defined( OP_PROBE3 ) && ( ENABLED_OP_PROBE3 )
# define OP_PROBE3(loc) OP_PROBE_INTERNAL(3, loc, #loc)
# define OP_PROBE3_PARAM(loc,par) OP_PROBE_INTERNAL_WITH_PARAM(0, loc, #loc, par)
#elif !defined( OP_PROBE3 )
# define OP_PROBE3(loc)
# define OP_PROBE3_PARAM(loc,par)
#endif

#if !defined( OP_PROBE4 ) && ( ENABLED_OP_PROBE4 )
# define OP_PROBE4(loc) OP_PROBE_INTERNAL(4, loc, #loc)
# define OP_PROBE4_PARAM(loc,par) OP_PROBE_INTERNAL_WITH_PARAM(0, loc, #loc, par)
#elif !defined( OP_PROBE4 )
# define OP_PROBE4(loc)
# define OP_PROBE4_PARAM(loc,par)
#endif

#if !defined( OP_PROBE5 ) && ( ENABLED_OP_PROBE5 )
# define OP_PROBE5(loc) OP_PROBE_INTERNAL(5, loc, #loc)
# define OP_PROBE5_PARAM(loc,par) OP_PROBE_INTERNAL_WITH_PARAM(0, loc, #loc, par)
#elif !defined( OP_PROBE5 )
# define OP_PROBE5(loc)
# define OP_PROBE5_PARAM(loc,par)
#endif

#if !defined( OP_PROBE6 ) && ( ENABLED_OP_PROBE6 )
# define OP_PROBE6(loc) OP_PROBE_INTERNAL(6, loc, #loc)
# define OP_PROBE6_PARAM(loc,par) OP_PROBE_INTERNAL_WITH_PARAM(0, loc, #loc, par)
#elif !defined( OP_PROBE6 )
# define OP_PROBE6(loc)
# define OP_PROBE6_PARAM(loc,par)
#endif

#if !defined( OP_PROBE7 ) && ( ENABLED_OP_PROBE7 )
# define OP_PROBE7(loc) OP_PROBE_INTERNAL(7, loc, #loc)
# define OP_PROBE7_PARAM(loc,par) OP_PROBE_INTERNAL_WITH_PARAM(0, loc, #loc, par)
#elif !defined( OP_PROBE7 )
# define OP_PROBE7(loc)
# define OP_PROBE7_PARAM(loc,par)
#endif

#if !defined( OP_PROBE8 ) && ( ENABLED_OP_PROBE8 )
# define OP_PROBE8(loc) OP_PROBE_INTERNAL(8, loc, #loc)
# define OP_PROBE8_PARAM(loc,par) OP_PROBE_INTERNAL_WITH_PARAM(0, loc, #loc, par)
#elif !defined( OP_PROBE8 )
# define OP_PROBE8(loc)
# define OP_PROBE8_PARAM(loc,par)
#endif

#if !defined( OP_PROBE9 ) && ( ENABLED_OP_PROBE9 )
# define OP_PROBE9(loc) OP_PROBE_INTERNAL(9, loc, #loc)
# define OP_PROBE9_PARAM(loc,par) OP_PROBE_INTERNAL_WITH_PARAM(0, loc, #loc, par)
#elif !defined( OP_PROBE9 )
# define OP_PROBE9(loc)
# define OP_PROBE9_PARAM(loc,par)
#endif

/*
 * Use the following macros for any function that may LEAVE, or call a
 * function that does without catching, e.g. if the function should have
 * been named with an 'L' at the end and stack unwinding is needed.
 */

#if !defined( OP_PROBE0_L ) && ( ENABLED_OP_PROBE0_L )
# define OP_PROBE0_L(loc) OP_PROBE_INTERNAL_L(0, loc, #loc)
# define OP_PROBE0_PARAM_L(loc,par) OP_PROBE_INTERNAL_WITH_PARAM_L(0, loc, #loc, par)
#elif !defined( OP_PROBE0_L )
# define OP_PROBE0_L(loc)
# define OP_PROBE0_PARAM_L(loc, par)
#endif

#if !defined( OP_PROBE1_L ) && ( ENABLED_OP_PROBE1_L )
# define OP_PROBE1_L(loc) OP_PROBE_INTERNAL_L(1, loc, #loc)
# define OP_PROBE1_PARAM_L(loc,par) OP_PROBE_INTERNAL_WITH_PARAM_L(0, loc, #loc, par)
#elif !defined( OP_PROBE1_L )
# define OP_PROBE1_L(loc)
# define OP_PROBE1_PARAM_L(loc, par)
#endif

#if !defined( OP_PROBE2_L ) && ( ENABLED_OP_PROBE2_L )
# define OP_PROBE2_L(loc) OP_PROBE_INTERNAL_L(2, loc, #loc)
# define OP_PROBE2_PARAM_L(loc,par) OP_PROBE_INTERNAL_WITH_PARAM_L(0, loc, #loc, par)
#elif !defined( OP_PROBE2_L )
# define OP_PROBE2_L(loc)
# define OP_PROBE2_PARAM_L(loc, par)
#endif

#if !defined( OP_PROBE3_L ) && ( ENABLED_OP_PROBE3_L )
# define OP_PROBE3_L(loc) OP_PROBE_INTERNAL_L(2, loc, #loc)
# define OP_PROBE3_PARAM_L(loc,par) OP_PROBE_INTERNAL_WITH_PARAM_L(0, loc, #loc, par)
#elif !defined( OP_PROBE3_L )
# define OP_PROBE3_L(loc)
# define OP_PROBE3_PARAM_L(loc, par)
#endif

#if !defined( OP_PROBE4_L ) && ( ENABLED_OP_PROBE4_L )
# define OP_PROBE4_L(loc) OP_PROBE_INTERNAL_L(2, loc, #loc)
# define OP_PROBE4_PARAM_L(loc,par) OP_PROBE_INTERNAL_WITH_PARAM_L(0, loc, #loc, par)
#elif !defined( OP_PROBE4_L )
# define OP_PROBE4_L(loc)
# define OP_PROBE4_PARAM_L(loc, par)
#endif

#if !defined( OP_PROBE5_L ) && ( ENABLED_OP_PROBE5_L )
# define OP_PROBE5_L(loc) OP_PROBE_INTERNAL_L(2, loc, #loc)
# define OP_PROBE5_PARAM_L(loc,par) OP_PROBE_INTERNAL_WITH_PARAM_L(0, loc, #loc, par)
#elif !defined( OP_PROBE5_L )
# define OP_PROBE5_L(loc)
# define OP_PROBE5_PARAM_L(loc, par)
#endif

#if !defined( OP_PROBE6_L ) && ( ENABLED_OP_PROBE6_L )
# define OP_PROBE6_L(loc) OP_PROBE_INTERNAL_L(2, loc, #loc)
# define OP_PROBE6_PARAM_L(loc,par) OP_PROBE_INTERNAL_WITH_PARAM_L(0, loc, #loc, par)
#elif !defined( OP_PROBE6_L )
# define OP_PROBE6_L(loc)
# define OP_PROBE6_PARAM_L(loc, par)
#endif

#if !defined( OP_PROBE7_L ) && ( ENABLED_OP_PROBE7_L )
# define OP_PROBE7_L(loc) OP_PROBE_INTERNAL_L(2, loc, #loc)
# define OP_PROBE7_PARAM_L(loc,par) OP_PROBE_INTERNAL_WITH_PARAM_L(0, loc, #loc, par)
#elif !defined( OP_PROBE7_L )
# define OP_PROBE7_L(loc)
# define OP_PROBE7_PARAM_L(loc, par)
#endif

#if !defined( OP_PROBE8_L ) && ( ENABLED_OP_PROBE8_L )
# define OP_PROBE8_L(loc) OP_PROBE_INTERNAL_L(2, loc, #loc)
# define OP_PROBE8_PARAM_L(loc,par) OP_PROBE_INTERNAL_WITH_PARAM_L(0, loc, #loc, par)
#elif !defined( OP_PROBE8_L )
# define OP_PROBE8_L(loc)
# define OP_PROBE8_PARAM_L(loc, par)
#endif

#if !defined( OP_PROBE9_L ) && ( ENABLED_OP_PROBE9_L )
# define OP_PROBE9_L(loc) OP_PROBE_INTERNAL_L(2, loc, #loc)
# define OP_PROBE9_PARAM_L(loc,par) OP_PROBE_INTERNAL_WITH_PARAM_L(0, loc, #loc, par)
#elif !defined( OP_PROBE9_L )
# define OP_PROBE9_L(loc)
# define OP_PROBE9_PARAM_L(loc, par)
#endif

#else // SUPPORT_PROBETOOLS

#define OP_PROBE0(loc)
#define OP_PROBE1(loc)
#define OP_PROBE2(loc)
#define OP_PROBE3(loc)
#define OP_PROBE4(loc)
#define OP_PROBE5(loc)
#define OP_PROBE6(loc)
#define OP_PROBE7(loc)
#define OP_PROBE8(loc)
#define OP_PROBE9(loc)

#define OP_PROBE0_PARAM(loc,par)
#define OP_PROBE1_PARAM(loc,par)
#define OP_PROBE2_PARAM(loc,par)
#define OP_PROBE3_PARAM(loc,par)
#define OP_PROBE4_PARAM(loc,par)
#define OP_PROBE5_PARAM(loc,par)
#define OP_PROBE6_PARAM(loc,par)
#define OP_PROBE7_PARAM(loc,par)
#define OP_PROBE8_PARAM(loc,par)
#define OP_PROBE9_PARAM(loc,par)

#define OP_PROBE0_L(loc)
#define OP_PROBE1_L(loc)
#define OP_PROBE2_L(loc)
#define OP_PROBE3_L(loc)
#define OP_PROBE4_L(loc)
#define OP_PROBE5_L(loc)
#define OP_PROBE6_L(loc)
#define OP_PROBE7_L(loc)
#define OP_PROBE8_L(loc)
#define OP_PROBE9_L(loc)

#define OP_PROBE0_PARAM_L(loc,par)
#define OP_PROBE1_PARAM_L(loc,par)
#define OP_PROBE2_PARAM_L(loc,par)
#define OP_PROBE3_PARAM_L(loc,par)
#define OP_PROBE4_PARAM_L(loc,par)
#define OP_PROBE5_PARAM_L(loc,par)
#define OP_PROBE6_PARAM_L(loc,par)
#define OP_PROBE7_PARAM_L(loc,par)
#define OP_PROBE8_PARAM_L(loc,par)
#define OP_PROBE9_PARAM_L(loc,par)

#endif // SUPPORT_PROBETOOLS

#endif // _PROBETOOLS_PROBEPOINTS_H
