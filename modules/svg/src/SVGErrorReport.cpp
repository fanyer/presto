/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */
#include "core/pch.h"

#ifdef SVG_SUPPORT
#ifdef SVG_LOG_ERRORS

#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/SVGErrorReport.h"
#include "modules/svg/src/SVGManagerImpl.h"

void
SVGErrorReport::Report(const uni_char* format, ...) const 
{
	OpString16 a;
	va_list arglist;
	va_start(arglist, format);
	a.AppendVFormat(format, arglist);
	va_end(arglist);
	g_svg_manager_impl->ReportError(m_elm, a.CStr(), m_doc);
}

void
SVGErrorReport::ReportWithHref(const uni_char* format, ...) const
{
	OpString16 a;
	va_list arglist;
	va_start(arglist, format);
	a.AppendVFormat(format, arglist);
	va_end(arglist);
	g_svg_manager_impl->ReportError(m_elm, a.CStr(), m_doc, TRUE);
}

#endif // SVG_LOG_ERRORS
#endif // SVG_SUPPORT
