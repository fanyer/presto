/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef OP_SCOPE_DOCUMENT_LISTENER_H
#define OP_SCOPE_DOCUMENT_LISTENER_H

#if defined(SCOPE_DOCUMENT_MANAGER) || defined(SCOPE_PROFILER)

#include "modules/dochand/docman.h"

class OpScopeDocumentListener
{
public:

	/**
	 * Arguments for OnAboutToLoadDocument. The contents copy the parameters
	 * of DocumentManager::OpenURL.
	 */
	struct AboutToLoadDocumentArgs
	{
		DocumentManager::OpenURLOptions options;
		URL_Rep *url;
		URL_Rep *referrer_url;
		BOOL check_if_expired;
		BOOL reload;
	}; // AboutToLoadDocumentArgs

	/**
	 * Check if document listener is enabled. If it isn't, the caller
	 * does not need to send events.
	 *
	 * @return TRUE if enabled, FALSE if disabled.
	 */
	static BOOL IsEnabled();

	/**
	 * Call this when a document is about to be loaded. (Not when it
	 * has finished loading).
	 *
	 * @param docman The DocumentManager that is about to be loaded.
	 * @param args Arguments to DocumentManager::OpenURL. See OnLoadDocumentArgs.
	 */
	static OP_STATUS OnAboutToLoadDocument(DocumentManager *docman, const AboutToLoadDocumentArgs &args);

}; // OpScopeDocumentListener

#endif // SCOPE_DOCUMENT_MANAGER || SCOPE_PROFILER

#endif // OP_SCOPE_DOCUMENT_LISTENER_H
