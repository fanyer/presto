/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
   Copyright 2005-2011 Opera Software ASA.  All rights Reserved.

   Cross-module security manager: Origin checking
   Lars T Hansen.  */

#include "core/pch.h"
#include "modules/doc/frm_doc.h"
#include "modules/dochand/win.h"
#include "modules/dochand/docman.h"
#include "modules/dom/domutils.h"
#include "modules/security_manager/include/security_manager.h"
#include "modules/pi/network/OpSocketAddress.h"
#include "modules/url/url_sn.h"

static void
GetDomainFromRuntime(DOM_Runtime *runtime, const uni_char **domain, URLType *type, int *port, const ServerName **sn)
{
	*domain = DOM_Utils::GetDomain(runtime);
	URL url = DOM_Utils::GetOriginURL(runtime);
	*type = static_cast<URLType>(url.GetAttribute(URL::KType, FALSE));
	*port = static_cast<int>(url.GetAttribute(URL::KServerPort, FALSE));
	*sn = static_cast<const ServerName *>(url.GetAttribute(URL::KServerName, NULL, FALSE));
}

static int
GetNetTypeFromServerName(const ServerName *sn)
{
	if (sn)
		return (int) sn->GetNetType();
	else
		return INT_MIN;
}

/* static */ BOOL
OpSecurityManager::OriginCheck(const OpSecurityContext& source, const OpSecurityContext& target)
{
	BOOL allowed = FALSE;
	const URL &source_url = source.GetURL();
	const URL &target_url = target.GetURL();
	URLType target_type = URL_UNKNOWN;
	URLType source_type = URL_UNKNOWN;

	const ServerName *target_sn = NULL;
	const ServerName *source_sn = NULL;

	const uni_char *target_domain = UNI_L("target_domain");
	const uni_char *source_domain = UNI_L("source_domain");
	int target_port = 0;
	int source_port = 0;

	DOM_Runtime *source_runtime = source.GetRuntime();
	DOM_Runtime *target_runtime = target.GetRuntime();

	if (source_runtime != NULL)
	{
		if (DOM_Utils::HasDebugPrivileges(source_runtime))
			return TRUE;

		BOOL source_domain_overridden = DOM_Utils::HasOverriddenDomain(source_runtime);

		if (target_runtime != NULL)
		{
			FramesDocument *source_doc = DOM_Utils::GetDocument(source_runtime);
			FramesDocument *target_doc = DOM_Utils::GetDocument(target_runtime);

			if (source_doc && target_doc && source_doc->ContainsOnlyOperaInternalScripts() != target_doc->ContainsOnlyOperaInternalScripts())
				return FALSE;
		}

		if (source_runtime == target_runtime)
			return TRUE;

		GetDomainFromRuntime(source_runtime, &source_domain, &source_type, &source_port, &source_sn);

		BOOL target_domain_overridden = FALSE;

		if (target_runtime != NULL)
		{
			GetDomainFromRuntime(target_runtime, &target_domain, &target_type, &target_port, &target_sn);
			target_domain_overridden = DOM_Utils::HasOverriddenDomain(target_runtime);
		}
		else if (target_url.IsValid())
			GetDomainFromURL(target_url, &target_domain, &target_type, &target_port, &target_sn);

		allowed = OriginCheck(target_type, target_port, target_domain, target_domain_overridden, GetNetTypeFromServerName(target_sn), source_type, source_port, source_domain, source_domain_overridden, GetNetTypeFromServerName(source_sn));
	}
	else if (source_url.IsValid() && target_url.IsValid())
	{
		source_type = source_url.Type();
		target_type = target_url.Type();
		allowed = OriginCheck(source_url, target_url);
	}

	if (allowed)
	{
		// Adjust for "file:" protocol: disallow access between URLs that
		// are not both files or both directories.  See bug #179578.

		if (source_type == URL_FILE && target_type == URL_FILE)
		{
			if (source_url.IsEmpty() || target_url.IsEmpty())
				allowed = FALSE;
			else
				allowed = source_url.GetAttribute(URL::KIsDirectoryListing, TRUE) == target_url.GetAttribute(URL::KIsDirectoryListing, TRUE);
		}
	}

	if (allowed && source_url.IsValid() && target_url.IsValid() && target_runtime)
	{
		FramesDocument *source_doc = DOM_Utils::GetDocument(source_runtime);
		FramesDocument *target_doc = DOM_Utils::GetDocument(target_runtime);
		if (source_doc && target_doc && source_doc->IsGeneratedByOpera() != target_doc->IsGeneratedByOpera())
			return FALSE;
	}

	return allowed;
}

/* static */ BOOL
OpSecurityManager::OriginCheck(const URL& url1, const URL& url2)
{
	return OriginCheck(url1.Type(), url1.GetServerPort(), url1.GetAttribute(URL::KUniHostName), FALSE, GetNetTypeFromServerName(static_cast<const ServerName *>(url1.GetAttribute(URL::KServerName, NULL, TRUE))),
	                   url2.Type(), url2.GetServerPort(), url2.GetAttribute(URL::KUniHostName), FALSE, GetNetTypeFromServerName(static_cast<const ServerName *>(url2.GetAttribute(URL::KServerName, NULL, TRUE))));
}

/* static */ BOOL
OpSecurityManager::OriginCheck(URLType type1, unsigned int port1, const uni_char *domain1, URLType type2, unsigned int port2, const uni_char *domain2)
{
	return OriginCheck(type1, port1, domain1, FALSE, 0, type2, port2, domain2, FALSE, 0);
}

/* static */ BOOL
OpSecurityManager::OriginCheck(URLType type1, unsigned int port1, const uni_char *domain1, BOOL domain1_overridden, int nettype1, URLType type2, unsigned int port2, const uni_char *domain2, BOOL domain2_overridden, int nettype2)
{
	/* Protocols must be the same */

	if (type1 != type2)
		return FALSE;

	if (domain1_overridden != domain2_overridden)
		/* One is overridden via document.domain but not the other => don't
		   accept.  Both document's must agree to override to the same domain
		   for the override to take effect. */
		return FALSE;

	/* Ports must normally be the same, after resolving
	   0 to default for protocol. The exception is when
	   the domain is overriden. Then ports no longer matter.
	   See bug 186562. */
	if (!domain1_overridden)
	{
		ResolveDefaultPort(type1, port1);
		ResolveDefaultPort(type2, port2);

		if (port1 != port2)
			return FALSE;
	}

	/* Network types (public/internet vs private/internal) must be the same */

	if (nettype1 != nettype2 && (nettype1 == NETTYPE_PUBLIC || nettype2 == NETTYPE_PUBLIC) && nettype1 != NETTYPE_UNDETERMINED && nettype2 != NETTYPE_UNDETERMINED)
		return FALSE;

	/* Domains must be the same, after resolving aliases for localhost to empty string */

	ResolveLocalDomain(domain1);
	ResolveLocalDomain(domain2);

	if (uni_strcmp(domain1, domain2) != 0)
		return FALSE;

	/* Accept */

	return TRUE;
}

/* static */ void
OpSecurityManager::ResolveDefaultPort(URLType t, unsigned int& p)
{
	if (p == 0)
	{
		switch (t)
		{
		case URL_HTTP:  p =  80; break;
		case URL_HTTPS: p = 443; break;
		case URL_FTP:   p =  21; break;
		}
	}
}

/* static */ void
OpSecurityManager::ResolveLocalDomain(const uni_char*& domain)
{
	if (domain == NULL || uni_str_eq(domain, "localhost"))
		domain = UNI_L("");
}

/* static */ URL
GetURLFromDocument(FramesDocument *document)
{
	return document->GetOrigin()->security_context;
}

/* static */ BOOL
OpSecurityManager::LocalOriginCheck(FramesDocument *document1, FramesDocument *document2)
{
	const uni_char *domain1, *domain2;
	BOOL domain1_overridden = FALSE, domain2_overridden = FALSE;
	URLType type1, type2;
	int port1, port2;
	const ServerName *sn1, *sn2;

	if (document1 == NULL || document2 == NULL)
		return FALSE;

	if (ES_Runtime *runtime1 = document1->GetESRuntime())
	{
		GetDomainFromRuntime(DOM_Utils::GetDOM_Runtime(runtime1), &domain1, &type1, &port1, &sn1);
		domain1_overridden = DOM_Utils::HasOverriddenDomain(DOM_Utils::GetDOM_Runtime(runtime1));
	}
	else
		GetDomainFromURL(GetURLFromDocument(document1), &domain1, &type1, &port1, &sn1);

	if (ES_Runtime *runtime2 = document2->GetESRuntime())
	{
		GetDomainFromRuntime(DOM_Utils::GetDOM_Runtime(runtime2), &domain2, &type2, &port2, &sn2);
		domain2_overridden = DOM_Utils::HasOverriddenDomain(DOM_Utils::GetDOM_Runtime(runtime2));
	}
	else
		GetDomainFromURL(GetURLFromDocument(document2), &domain2, &type2, &port2, &sn2);

	return OriginCheck(type1, port1, domain1, domain1_overridden, GetNetTypeFromServerName(sn1), type2, port2, domain2, domain2_overridden, GetNetTypeFromServerName(sn2));
}

/* static */ FramesDocument*
OpSecurityManager::GetOwnerDocument(FramesDocument *document)
{
	if (FramesDocument *parent = document->GetParentDoc())
		return parent;

	if (FramesDocument *opener = static_cast<FramesDocument*>(document->GetWindow()->GetOpener()))
		if (opener->GetSecurityContext() == document->GetWindow()->GetOpenerSecurityContext())
			return opener;

	return NULL;
}

/* static */ void
OpSecurityManager::GetDomainFromURL(const URL &url, const uni_char **domain, URLType *type, int *port, const ServerName **sn)
{
	*type = (URLType) url.GetAttribute(URL::KType, TRUE);
	*port = (int) url.GetAttribute(URL::KServerPort, TRUE);
	*sn = static_cast<const ServerName *>(url.GetAttribute(URL::KServerName, NULL, FALSE));

	if (*sn)
		*domain = (*sn)->UniName();
	else
		*domain = UNI_L("");
}

/* static */ BOOL
OpSecurityManager::OperaUrlCheck(const URL& source)
{
	/* opera:config has relaxed security */

	const char *path = source.GetAttribute(URL::KPath);
	URLType type = (URLType) source.GetAttribute(URL::KType, TRUE);

	if (type == URL_OPERA && path && op_stricmp(path, "config") == 0)
		return TRUE;
	else
		return FALSE;
}
