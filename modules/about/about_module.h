/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#ifndef MODULES_ABOUT_ABOUT_MODULE_H
#define MODULES_ABOUT_ABOUT_MODULE_H

class OperaDebugProxy;

class AboutModule : public OperaModule
{
public:
	AboutModule();

	virtual void InitL(const OperaInitInfo& info);
	virtual void Destroy();

#ifdef ABOUT_HTML_DIALOGS
	class HTMLDialogManager* m_html_dialog_manager;
#endif // ABOUT_HTML_DIALOGS

#ifdef ERROR_PAGE_SUPPORT
	class OpIllegalHostPageURL_Generator *m_illegal_host_page;
#endif
};

#define ABOUT_MODULE_REQUIRED

#endif // MODULES_ABOUT_ABOUT_MOULE_H
