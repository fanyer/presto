/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#if defined DEBUG_ENABLE_OPASSERT || defined _DEBUG
/* static */ const char*
OpStatus::Stringify(const OP_STATUS_ARG value)
{
#define OP_STATUS_CASE(v) case v: return "OpStatus::" #v;
	switch (OpStatus::GetIntValue(value))
	{
	OP_STATUS_CASE(OK)
	OP_STATUS_CASE(ERR)
	OP_STATUS_CASE(ERR_NO_MEMORY)
	OP_STATUS_CASE(ERR_NULL_POINTER)
	OP_STATUS_CASE(ERR_OUT_OF_RANGE)
	OP_STATUS_CASE(ERR_NO_ACCESS)
	OP_STATUS_CASE(ERR_NOT_DELAYED)
	OP_STATUS_CASE(ERR_FILE_NOT_FOUND)
	OP_STATUS_CASE(ERR_NO_DISK)
	OP_STATUS_CASE(ERR_NOT_SUPPORTED)
	OP_STATUS_CASE(ERR_PARSING_FAILED)
	OP_STATUS_CASE(ERR_NO_SUCH_RESOURCE)
	OP_STATUS_CASE(ERR_BAD_FILE_NUMBER)
	OP_STATUS_CASE(ERR_YIELD)
	OP_STATUS_CASE(ERR_SOFT_NO_MEMORY)
	}
#undef OP_STATUS_CASE
	return NULL;
}

/* static */ const char*
OpBoolean::Stringify(const OP_STATUS_ARG value)
{
#define OP_BOOLEAN_CASE(v) case v: return "OpBoolean::" #v;
	switch (OpStatus::GetIntValue(value))
	{
	OP_BOOLEAN_CASE(IS_FALSE)
	OP_BOOLEAN_CASE(IS_TRUE)
	}
#undef OP_BOOLEAN_CASE
	return OpStatus::Stringify(value);
}
#endif // defined DEBUG_ENABLE_OPASSERT || defined _DEBUG

#ifdef USE_DEBUGGING_OP_STATUS

#if defined(HAVE_LTH_MALLOC)
#  define RECORD_ALLOC_SITE
#  include "modules/memtools/happy-malloc.h"
#  ifdef STACK_TRACE_SUPPORT
#    include "modules/memtools/stacktrace.h"
#  ifdef ADDR2LINE_SUPPORT
#    include "modules/memtools/addr2line.h"
#    include "modules/memtools/mem_util.h"
#  endif // ADDR2LINE_SUPPORT
#  endif
#endif

/**
 * Called to output debug message when OP_STATUS value is unchecked,
 * or someone is reading an undefined value.
 */
void
#if defined(HAVE_LTH_MALLOC)
OP_STATUS::Error(const char *msg, site_t *site) const
#else
OP_STATUS::Error(const char *msg) const
#endif
{
//	fprintf(stderr, "error: %s\n", msg);
#if defined(HAVE_LTH_MALLOC)
	if (malloc_record_static_opstatus_failure(*site) == 0)
	{
#ifdef ADDR2LINE_SUPPORT
		if (MemUtil::MatchesModule(m_construct_addrs, STACK_LEVELS))
        {
#endif // ADDR2LINE_SUPPORT
# ifdef STACK_TRACE_SUPPORT
		fprintf(stderr, "***OP_STATUS ERROR: %s\n", msg);
		fprintf(stderr, "   -------- Created At -----------\n");
		show_stored_stack_trace(m_construct_addrs, STACK_LEVELS);
		fprintf(stderr, "   -------- Deleted At -----------\n");
		show_stack_trace_from(STACK_LEVELS, site->location);
		fprintf(stderr, "   -------------------------------\n\n");
# endif
#ifdef ADDR2LINE_SUPPORT
		}
#endif // ADDR2LINE_SUPPORT
		 int set_breakpoint_here = 0;
	}
#endif
	// Not sure if we should crash or just report the fact.  For now,
	// I disable the crash. [pere 2001-05-28]
#if defined(FORCE_OPSTATUS_CHECK)
#if defined(UNIX)
	const char *env = getenv("OPERA_IGNORE_OP_STATUS");
	if (NULL != env)
		return;
#endif // UNIX
#if !defined(NDEBUG)
	assert(!"Unchecked OP_STATUS value!");
#else // NDEBUG
	LEAVE(OpStatus::ERR);
#endif // NDEBUG
#endif // FORCE_OPSTATUS_CHECK
}

int OP_STATUS::m_ignored   = 0;
int OP_STATUS::m_undefined = 0;

#if defined(HAVE_LTH_MALLOC)
OP_STATUS::~OP_STATUS()
{
#ifdef PARTIAL_LTH_MALLOC
	if (!(in_non_oom_code() && created_in_non_oom_code))
#endif
	{

		LTH_MALLOC_SITE(site, 0);

		if (m_status == UNCHECKED) {
			Error("OP_STATUS::IgnoredOpStatus(): Unchecked OP_STATUS value!", &site);
			m_ignored++;
		}
	}
}
#endif

/**
 * Called when an OP_STATUS object is destroyed without anyone
 * checking the value of the object.  Only used during debugging.
 */
void
OP_STATUS::IgnoredOpStatus(void) const
{
	// Let all hell break loose, unchecked return value!
#if !defined(HAVE_LTH_MALLOC)
	Error("OP_STATUS::IgnoredOpStatus(): Unchecked OP_STATUS value!");
	m_ignored++;
#endif
}

/**
 * Called when the value of an OP_STATUS object is checked before the
 * value is defined.
 */
void
OP_STATUS::UndefinedOpStatus() const
{
	// Error("OP_STATUS::UndefinedOpStatus(): Undefined OP_STATUS value!");
	m_undefined++;
}

/**
 * Return the number of ignored OP_STATUS values.  Used to test this
 * class.
 */
int
OP_STATUS::GetIgnored()
{
	return m_ignored;
}

/**
 * Return the number of undefined OP_STATUS values checked.  Used to
 * test this class.
 */
int
OP_STATUS::GetUndefined()
{
	return m_undefined;
}

#endif // USE_DEBUGGING_OP_STATUS
