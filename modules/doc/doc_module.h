/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_DOC_DOC_MODULE_H
#define MODULES_DOC_DOC_MODULE_H

#include "modules/util/adt/opvector.h"

class FramesDocument;
class ReadyStateListener;
class DocOperaStyleURLManager;
class DocumentMenuManager;

class DocModule : public OperaModule
{
public:
	DocModule();

	virtual void InitL(const OperaInitInfo& info);
	virtual void Destroy();

	DocOperaStyleURLManager *opera_style_url_generator;

	unsigned int unique_origin_counter_high;
	unsigned int unique_origin_counter_low;

	BOOL always_create_dom_environment;

#ifndef HAS_NOTEXTSELECTION
	/**
	 * If an expanding selection should trigger a scroll.
	 *
	 * @todo Don't need a global variable for this.
	 */
	BOOL		m_selection_scroll_active;

	/**
	 * The document that should scroll.
	 */
	FramesDocument*	m_selection_scroll_document;
#endif // !HAS_NOTEXTSELECTION

	OpVector<ReadyStateListener> m_ready_state_listeners;

#ifdef _OPERA_DEBUG_DOC_
	unsigned m_total_document_count;
#endif // _OPERA_DEBUG_DOC_

	DocumentMenuManager* m_menu_manager;
};

#ifndef HAS_NOTEXTSELECTION
# define selection_scroll_active g_opera->doc_module.m_selection_scroll_active
#endif // !HAS_NOTEXTSELECTION

#define g_menu_manager (g_opera->doc_module.m_menu_manager)

#define DOC_MODULE_REQUIRED

#endif // !MODULES_DOC_DOC_MODULE_H
