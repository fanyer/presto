/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2000-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/
#include "core/pch.h"

#include "modules/about/about_module.h"
#ifdef ABOUT_HTML_DIALOGS
# include "modules/about/ophtmldialogs.h"
# include "modules/dom/domenvironment.h"
#endif // ABOUT_HTML_DIALOGS
#ifdef ERROR_PAGE_SUPPORT
#include "modules/about/opillegalhostpage.h"
#endif

AboutModule::AboutModule()
	: OperaModule() // Makes #if-ery and punctuation co-operate better
#ifdef ABOUT_HTML_DIALOGS
	, m_html_dialog_manager(NULL)
#endif // ABOUT_HTML_DIALOGS
#ifdef ERROR_PAGE_SUPPORT
	, m_illegal_host_page(NULL)
#endif
{
}

void AboutModule::InitL(const OperaInitInfo& info)
{
#ifdef ABOUT_HTML_DIALOGS
	m_html_dialog_manager = OP_NEW_L(HTMLDialogManager, ());
#endif // ABOUT_HTML_DIALOGS
#ifdef ERROR_PAGE_SUPPORT
	m_illegal_host_page = OP_NEW_L(OpIllegalHostPageURL_Generator, ());
	LEAVE_IF_ERROR(m_illegal_host_page->Construct("illegal-url-", TRUE));
	g_url_api->RegisterOperaURL(m_illegal_host_page);
#endif

}

void AboutModule::Destroy()
{
#ifdef ABOUT_HTML_DIALOGS
	OP_DELETE(m_html_dialog_manager);
	m_html_dialog_manager = NULL;
#endif // ABOUT_HTML_DIALOGS
#ifdef ERROR_PAGE_SUPPORT
	OP_DELETE(m_illegal_host_page);
	m_illegal_host_page = NULL;
#endif
}
