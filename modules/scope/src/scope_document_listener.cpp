/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#if defined(SCOPE_DOCUMENT_MANAGER) || defined(SCOPE_PROFILER)

#include "modules/scope/scope_document_listener.h"
#include "modules/scope/src/scope_manager.h"
#include "modules/scope/src/scope_document_manager.h"
#include "modules/scope/src/scope_profiler.h"

/* static */ BOOL
OpScopeDocumentListener::IsEnabled()
{
	if (g_scope_manager)
	{
#ifdef SCOPE_DOCUMENT_MANAGER
		if (g_scope_manager->document_manager && g_scope_manager->document_manager->IsEnabled())
			return TRUE;
#endif // SCOPE_DOCUMENT_MANAGER
#ifdef SCOPE_PROFILER
		if (g_scope_manager->profiler && g_scope_manager->profiler->IsEnabled())
			return TRUE;
#endif // SCOPE_PROFILER
	}

	return FALSE;
}

/* static */ OP_STATUS
OpScopeDocumentListener::OnAboutToLoadDocument(DocumentManager *docman, const AboutToLoadDocumentArgs &args)
{
	if (g_scope_manager)
	{
#ifdef SCOPE_DOCUMENT_MANAGER
		if (g_scope_manager->document_manager)
			RETURN_IF_MEMORY_ERROR(g_scope_manager->document_manager->OnAboutToLoadDocument(docman, args));
#endif // SCOPE_DOCUMENT_MANAGER
#ifdef SCOPE_PROFILER
		if (g_scope_manager->profiler)
			return g_scope_manager->profiler->OnAboutToLoadDocument(docman, args);
#endif // SCOPE_PROFILER
	}

	return OpStatus::OK;
}

#endif // SCOPE_DOCUMENT_MANAGER || SCOPE_PROFILER

