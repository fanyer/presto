/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef _PROBEPOINT_ENABLING_H
#define _PROBEPOINT_ENABLING_H

/*
PROBE LEVELS:

Please follow the following directions:

OP_PROBE0: Spartan's probes for automated regression testing. This includes probes of all core messages. Adding new level 0 probes can alter self time for existing probes, so it is good to check how probe data is used on Spartan before adding new ones.
OP_PROBE1: Startup probes. 
OP_PROBE2: Common platform-sensitive low-level functionality like file I/O, sockets and paint events
OP_PROBE3: Module high level APIs to measure module interactions, for example to track who initializes REFLOWs
OP_PROBE4 - OP_PROBE6: High level module functions (module owner decides, 4 should call 5, should call 6)
OP_PROBE7 - OP_PROBE9: Low level probes for testing and developement. These probe profiles should not be enabled in HEAD

Note: OP_PROBE0 and OP_PROBE1 should not exceed 1% overhead in the core profiles
*/

#ifdef SUPPORT_PROBETOOLS

// Enable (1) or disable (0) the "ordinary" probes level 0 through 9:
#define ENABLED_OP_PROBE0 1	//SPARTAN
#define ENABLED_OP_PROBE1 0	//STARTUP
#define ENABLED_OP_PROBE2 0 //Platform-dependent probes. FILE + PAINT + SOCKET(TODO)
#define ENABLED_OP_PROBE3 0
#define ENABLED_OP_PROBE4 0
#define ENABLED_OP_PROBE5 0
#define ENABLED_OP_PROBE6 0
#define ENABLED_OP_PROBE7 0
#define ENABLED_OP_PROBE8 0
#define ENABLED_OP_PROBE9 0

/*
 * Use the following macros for any function that may LEAVE, or call a
 * function that does without catching, e.g. if the function should have
 * been named with an 'L' at the end and stack unwinding is needed.
 * Enable (1) or disable (0) these L-probes level 0 through 9:
 */
#define ENABLED_OP_PROBE0_L 1
#define ENABLED_OP_PROBE1_L 0
#define ENABLED_OP_PROBE2_L 0
#define ENABLED_OP_PROBE3_L 0
#define ENABLED_OP_PROBE4_L 0
#define ENABLED_OP_PROBE5_L 0
#define ENABLED_OP_PROBE6_L 0
#define ENABLED_OP_PROBE7_L 0
#define ENABLED_OP_PROBE8_L 0
#define ENABLED_OP_PROBE9_L 0

/*
 * Set ENABLED_OP_PROBE_LOG to non-zero to get a probe log, basically
 * a list of <timestamp, probe id> tuples, written to
 * <OPFILE_TEMP_FOLDER>/probedata/probelog.txt.  The log is
 * automatically overwritten each time the executable exits.
 */
#define ENABLED_OP_PROBE_LOG 0
/*
 * Maximum number of stored invocations.  Increase this value if you
 * have the need and memory to spare.
 */
#define PROBE_MAX_STORED_INVOCATIONS 10000

#endif // SUPPORT_PROBETOOLS
#endif // _PROBEPOINT_LEVELS_H
