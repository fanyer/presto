/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright 2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#include "modules/dochand/docman.h"
#include "modules/gadgets/OpGadgetClass.h"
#include "modules/gadgets/OpGadget.h"
#include "modules/security_manager/include/security_manager.h"
#include "modules/url/url2.h"

BOOL OpSecurityContext_Docman::IsTopDocument() const
{
	return document_manager && document_manager->GetParentDoc() == NULL;
}

OP_STATUS OpSecurityManager_Docman::CheckDocManUrlLoadingSecurity(const OpSecurityContext& source, const OpSecurityContext& target, OpSecurityCheckCallback* callback)
{
	BOOL allowed = TRUE;

	const URL &source_url = source.GetURL();
	const URL &target_url = target.GetURL();

#ifdef GADGET_SUPPORT
	if (source.IsGadget())
		return g_secman_instance->CheckGadgetUrlLoadSecurity(source, target, callback);
#endif // GADGET_SUPPORT

	OP_ASSERT(source.GetDocumentManager());

	if (source_url.IsEmpty())
		allowed = TRUE;
	else if (target_url.Type() == URL_FILE)
	{
		if (source_url.Type() == URL_OPERA)
		{
			// Some opera: urls may load (frames) from file
			OpString8 opera_page;
			OpStatus::Ignore(source_url.GetAttribute(URL::KPath, opera_page));
			if (opera_page.CompareI("drives") == 0 ||
				opera_page.CompareI("cache", 5) == 0 ||
				opera_page.CompareI("history") == 0 ||
				opera_page.CompareI("speeddial") == 0 ||
#ifdef AB_LGPL_NOTICE_GLIBC
// The LGPL notice has a file: link to the installed LGPL file, so
// allow opening that file from the opera:about page:
				opera_page.CompareI("about") == 0 ||
#endif // ABOUT_GLIBC
				opera_page.CompareI("config") == 0)
			{
				// Pages we trust
				allowed = TRUE;
			}
			else
			{
				for (OperaURL_Generator *generator = g_OperaUrlGenerators.First(); generator; generator = generator->Suc())
					if (generator->MatchesPath(opera_page) && generator->IsAllowedFileURLAccess())
					{
						allowed = TRUE;
						break;
					}
			}
		}
		else
			allowed = source_url.Type() == URL_FILE; // Files can load files, nobody else except for exceptions above
	}
	else if (target_url.Type() == URL_OPERA && source_url.Type() != URL_OPERA && source.GetDocumentManager()->GetFrame())
	{
		// Don't allow loading opera urls in frames
		OpString8 path;
		if (OpStatus::IsError(target_url.GetAttribute(URL::KPath, path)))
			allowed = FALSE;
		else if (IsAboutBlankURL(target_url) ||
			path.CompareI(OPERA_CLICKJACK_BLOCK_URL_PATH, sizeof(OPERA_CLICKJACK_BLOCK_URL_PATH) - 1) == 0)
		{
			// All possible error pages and download pages should go here
			allowed = TRUE;
		}
		else
		{
			allowed = FALSE;
			// Pure numbers mean generated error pages. They should also be loadable.
			if (!path.IsEmpty())
			{
				char* end;
				long number = op_strtol(path.CStr(), &end, 10);
				if (path.Length() == end - path.CStr() && number >= 0)
					allowed = TRUE;
			}
		}
	}

#ifdef WEBSERVER_SUPPORT
	if (target_url.GetAttribute(URL::KIsUniteServiceAdminURL))
	{
		BOOL allowed;
		OP_STATUS status = g_secman_instance->CheckSecurity(OpSecurityManager::UNITE_STANDARD, source, target, allowed);
		if (OpStatus::IsError(status))
		{
			callback->OnSecurityCheckError(status);
			return status;
		}
	}
#endif // WEBSERVER_SUPPORT

	callback->OnSecurityCheckSuccess(allowed);
	return OpStatus::OK;
}
