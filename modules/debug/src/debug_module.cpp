/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2005-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

/** \file
 *
 * \brief Implement the global opera object for debug module.
 */

#include "core/pch.h"
#include "modules/debug/debug_module.h"

#ifdef DEBUG_MODULE_REQUIRED

DebugModule::DebugModule(void)
	: dbg_init_magic(DBG_INIT_MAGIC)
#ifdef DEBUG_ENABLE_OPTRACER
	, dbg_tracer_id(1)
#endif
#ifdef _DEBUG
	, dbg_level(0)
	, dbg_debugging(FALSE)
	, dbg_tracing(FALSE)
	, dbg_prefix(TRUE)
	, dbg_timing(FALSE)
	, dbg_timestamp(FALSE)
	, dbg_system_debug(FALSE)
	, dbg_use_file(FALSE)
	, dbg_console(FALSE)
	, dbg_all_keywords(FALSE)
	, dbg_filename(0)
	, dbg_out(0)
#endif // _DEBUG
#if defined(SELFTEST) && defined(DEBUG_ENABLE_PRINTF)
	, dbg_printf_selftest_data(0)
	, dbg_printf_selftest_data_size(0)
#endif
{
}

void DebugModule::InitL(const OperaInitInfo&)
{
#ifdef DEBUG_ENABLE_PRINTF
	// This empty log statement may trigger the creation of the logfile
	// (to avoid having the previous logfile stay around, if nothing is
	// written to the log).
	log_printf("");
#endif
}

void DebugModule::Destroy(void)
{
	//
	// Do nothing here.  Instead, call Debug::Free() from the platform
	// code just before making the global Opera object unavailable,
	// while the file-IO porting interface is still operational.
	//
}

#endif // DEBUG_MODULE_REQUIRED
