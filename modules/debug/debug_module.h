/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/** \file
 * \brief Declare the global opera object for debug module
 *
 * Various global variables used by the debug module goes into the
 * global opera object.  Some string arrays are located here for
 * instance, to provide temporary storage during formatting
 * operations and other where we don't want to use dynamic memory.
 *
 * \author Morten Rolland, moretnro@opera.com
 */

#ifndef DEBUG_MODULE_H
#define DEBUG_MODULE_H

#ifdef _DEBUG
// Debug methods needs the global variables
#define DEBUG_MODULE_REQUIRED
#endif

#ifdef DEBUG_ENABLE_PRINTF
// The log_printf method needs to be called from DebugModule::InitL() to
// initialize the platform output function (by trying to print nothing).
#undef  DEBUG_MODULE_REQUIRED
#define DEBUG_MODULE_REQUIRED
#endif

#ifdef DEBUG_MODULE_REQUIRED

#include "modules/debug/src/debug_internal.h"

// Magic cookie used to detect the validity of the debug global variables
#define DBG_INIT_MAGIC 0xdeb06a0c

/**
 * \brief Take care of initialization and global variables.
 *
 * All global variables are located in the global opera
 * object, with DebugModule as the container for the
 * debug module. All variables are prefixed with dbg_ in
 * this class, but must be referenced through the
 * corresponding macros with g_ prepended.
 */

class DebugModule : public OperaModule
{
public:
	/**
	 * \brief Constructing DebugModule to a well defined state.
	 *
	 * Initialization is to "default off", so the options in
	 * \ref dbg-debugtxt "debug.txt" basically adds the options
	 * it wants to enable.
	 */
	DebugModule(void);

	/**
	 * \brief Opera Object Initialization method.
	 *
	 * In the debug module, this method does not do anything,
	 * as most initialization is "on request".
	 */
	virtual void InitL(const OperaInitInfo& info);

	/**
	 * \brief Opera Object Destroy method.
	 *
	 * Free all resources used by the debug module.  This may cause
	 * valuable settings to disappear before debugging is completely
	 * done, but nothing can be done about this, unfortunately, without
	 * braking the fundamental idea of the opera object.
	 */
	virtual void Destroy(void);

	UINT32 dbg_init_magic; ///< Magic cookie to decide if module is healthy.

#ifdef DEBUG_ENABLE_OPTRACER
	int dbg_tracer_id;
#endif

#ifdef _DEBUG
	int dbg_level; ///< Indentation level when tracing.
	BOOL dbg_debugging;	///< True when debugging is enabled.
	BOOL dbg_tracing; ///< True when tracing is enabled.
	BOOL dbg_prefix; ///< True when prefixing output with keyword and function name.
	BOOL dbg_timing; ///< True when timing is enabled.
	BOOL dbg_timestamp; ///< True when output timestamps are enabled.
	BOOL dbg_system_debug; ///< True when output is sent to system output.
	BOOL dbg_use_file; ///< True when output is sent to file.
	BOOL dbg_console; ///< True when output is sent to the console.
	BOOL dbg_all_keywords; ///< True when all keywords to match.

	uni_char dbg_mybuff[DEBUG_DEBUGBUFFERSIZE]; // ARRAY OK 2007-03-09 mortenro

	char dbg_mycbuff[DEBUG_DEBUGBUFFERSIZE]; // ARRAY OK 2007-03-09 mortenro

	uni_char dbg_timebuff[DEBUG_DEBUGBUFFERSIZE]; // ARRAY OK 2007-03-09 mortenro
	char* dbg_filename; ///< Filename of output file.

	OpFile* dbg_out; ///< File handle for output to file.

	DebugHead dbg_keywords; ///< List of keywords for tracing
	DebugHead dbg_timers; ///< List of keywords for timing
#endif // _DEBUG

#if defined(SELFTEST) && defined(DEBUG_ENABLE_PRINTF)
	uni_char* dbg_printf_selftest_data; ///< Selftest buffer for dbg_printf
	int dbg_printf_selftest_data_size; ///< Size of dbg_printf test buffer
#endif
};

#ifdef DEBUG_ENABLE_OPTRACER
#define g_dbg_tracer_id		g_opera->debug_module.dbg_tracer_id
#endif

#ifdef _DEBUG

#define g_dbg_level			g_opera->debug_module.dbg_level
#define g_dbg_debugging		g_opera->debug_module.dbg_debugging
#define g_dbg_tracing		g_opera->debug_module.dbg_tracing
#define g_dbg_prefix		g_opera->debug_module.dbg_prefix
#define g_dbg_timing		g_opera->debug_module.dbg_timing
#define g_dbg_timestamp		g_opera->debug_module.dbg_timestamp
#define g_dbg_system_debug	g_opera->debug_module.dbg_system_debug
#define g_dbg_use_file		g_opera->debug_module.dbg_use_file
#define g_dbg_console		g_opera->debug_module.dbg_console
#define g_dbg_all_keywords	g_opera->debug_module.dbg_all_keywords
#define g_dbg_init_magic	g_opera->debug_module.dbg_init_magic

#define g_dbg_mybuff		g_opera->debug_module.dbg_mybuff
#define g_dbg_mycbuff		g_opera->debug_module.dbg_mycbuff
#define g_dbg_timebuff		g_opera->debug_module.dbg_timebuff
#define g_dbg_filename		g_opera->debug_module.dbg_filename

#define g_dbg_out			g_opera->debug_module.dbg_out

#define g_dbg_keywords		g_opera->debug_module.dbg_keywords
#define g_dbg_timers		g_opera->debug_module.dbg_timers

#endif // _DEBUG

#if defined(SELFTEST) && defined(DEBUG_ENABLE_PRINTF)
#define g_dbg_printf_selftest_data \
	g_opera->debug_module.dbg_printf_selftest_data
#define g_dbg_printf_selftest_data_size \
	g_opera->debug_module.dbg_printf_selftest_data_size
#endif

#endif // DEBUG_MODULE_REQUIRED

#endif // !DEBUG_MODULE_H
