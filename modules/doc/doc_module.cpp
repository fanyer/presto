/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/doc/doc_module.h"
#include "modules/doc/frm_doc.h"
#include "modules/doc/documentmenumanager.h"

DocModule::DocModule()
	: opera_style_url_generator(NULL)
	, unique_origin_counter_high(0)
	, unique_origin_counter_low(0)
	, always_create_dom_environment(FALSE)
#ifndef HAS_NOTEXTSELECTION
	, m_selection_scroll_active(FALSE)
	, m_selection_scroll_document(NULL)
#endif // !HAS_NOTEXTSELECTION
#ifdef _OPERA_DEBUG_DOC_
	, m_total_document_count(0)
#endif // _OPERA_DEBUG_DOC_
	, m_menu_manager(NULL)
{
}

/* virtual */ void
DocModule::InitL(const OperaInitInfo& info)
{
	m_menu_manager = OP_NEW_L(DocumentMenuManager, ());
}

/* virtual */ void
DocModule::Destroy()
{
	if (opera_style_url_generator)
	{
		OP_DELETE(opera_style_url_generator);
		opera_style_url_generator = NULL;
	}

	OP_DELETE(m_menu_manager);
	m_menu_manager = NULL;
}
