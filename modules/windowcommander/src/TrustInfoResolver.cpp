/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

/**
 * @file trust_info_resolver.cpp
 *
 * Contains function definitions for the classes defined in the corresponding
 * header file.
 */

#include "core/pch.h"

#ifdef TRUST_INFO_RESOLVER
# ifdef URL_FILTER
#  include "modules/content_filter/content_filter.h"
# endif // URL_FILTER
# include "modules/doc/doc.h"
# include "modules/doc/frm_doc.h"
# include "modules/dochand/win.h"
# include "modules/dochand/fraud_check.h"
# include "modules/pi/network/OpSocketAddress.h"
# include "modules/pi/OpLocale.h"
# include "modules/security_manager/include/security_manager.h"
# include "modules/url/url_socket_wrapper.h"
# include "modules/url/url_api.h"
# include "modules/url/url2.h"
# include "modules/windowcommander/OpTrustInfoResolver.h"
# include "modules/windowcommander/src/TrustInfoResolver.h"
# include "modules/windowcommander/src/WindowCommander.h"
# include "modules/xmlutils/xmlparser.h"
# include "modules/xmlutils/xmltoken.h"
# include "modules/xmlutils/xmltokenhandler.h"

TrustInformationResolver::~TrustInformationResolver()
{
	OP_DELETE(m_host_resolver);
}

OP_STATUS TrustInformationResolver::DownloadTrustInformation(OpWindowCommander * target_windowcommander)
{
	if (!target_windowcommander)
		return OpStatus::ERR;

	m_target_windowcommander = static_cast<WindowCommander *> (target_windowcommander);

	BOOL is_local, needs_resolving;
	ServerTrustChecker::IsLocalURL(m_server_url, is_local, needs_resolving);

	// If unresolved, start resolving
	if (needs_resolving)
	{
		// Start resolving.
		RETURN_IF_ERROR(SocketWrapper::CreateHostResolver(&m_host_resolver, this));
		RETURN_IF_ERROR(m_host_resolver->Resolve(m_server_url.GetServerName()->UniName()));
		return OpStatus::OK;
	}
	else
	{
		OP_STATUS status = OpenTrustInformation(is_local);
		if (OpStatus::IsSuccess(status) && m_listener)
			m_listener->OnTrustInfoResolved();
		return status;
	}
}

OP_STATUS TrustInformationResolver::OpenTrustInformation(BOOL is_local)
{
	URL trust_info_url;
	// Check if the address is an intranet page.
	if (!is_local)
	{
		// The site is not in the intranet range
		// Generate using core function exported from doc.
		RETURN_IF_ERROR(TrustInfoParser::GenerateRequestURL(m_server_url, trust_info_url, TRUE));
	}
	else
	{
		// The site is in the intranet range
		// Just get the static "can't check intranet" page.
		OpString8 request_string;
		request_string.AppendFormat("http://%s/", SITECHECK_HOST);
		request_string.AppendFormat("info/?intranet=yes");
		trust_info_url = g_url_api->GetURL(request_string.CStr());
	}

	// The url to get has been found.
	m_target_windowcommander->SetScriptingDisabled(TRUE);
	//Make an opstring with the url :
	OpString url_str;
	if ( (URLType)trust_info_url.GetAttribute(URL::KType) == URL_HTTPS )
		url_str.Set("https://");
	else
		url_str.Set("http://");
	url_str.Append(trust_info_url.GetServerName()->Name());

	OpString t_unipath_and_query;
	trust_info_url.GetAttribute(URL::KUniPathAndQuery, 0, t_unipath_and_query, URL::KNoRedirect);
	url_str.Append(t_unipath_and_query.CStr());

#ifdef URL_FILTER
	g_urlfilter->SetBlockMode(URLFilter::BlockModeOff); // Disable the content blocker
#endif // URL_FILTER

	//Resolve the url :
	OpString url_resolved;
	OP_STATUS r;
	BOOL resolved = FALSE;
	TRAP(r,resolved=g_url_api->ResolveUrlNameL(url_str, url_resolved));

	//Fix for Bug 232250 : To be able to call OpenURL and make sure that the url
	//                     is not added to history the OpenURL in the window is
	//                     used for it takes a URL. That means that we can make
	//                     sure that the URL is unique before we call OpenURL.
	if (resolved && OpStatus::IsSuccess(r))
	{
		URL ref_url;

		//Get a unique url - this will make sure that the url is not added to history (Bug 232250)
		URL url = g_url_api->GetURL(ref_url, url_resolved.CStr(), TRUE);
		url.SetAttribute(URL::KBlockUserInteraction, TRUE);


		m_target_windowcommander->GetWindow()->OpenURL(url,
													 DocumentReferrer(ref_url),
													 TRUE,
													 FALSE,
													 -1,
													 FALSE,
													 FALSE,
													 NotEnteredByUser,
													 FALSE);
	}
#ifdef URL_FILTER
	g_urlfilter->SetBlockMode(URLFilter::BlockModeNormal); // Enable the content blocker
#endif // URL_FILTER
	return OpStatus::OK;
}

void TrustInformationResolver::OnHostResolved(OpHostResolver* host_resolver)
{
	if (m_listener)
	{
		OpSocketAddress* socket_address = NULL;
		OpSocketAddress::Create(&socket_address);
		if (m_host_resolver->GetAddressCount() >= 1)
		{
			m_host_resolver->GetAddress(socket_address, 0);

			BOOL is_local, needs_resolving;
			ServerTrustChecker::IsLocalURL(m_server_url, is_local, needs_resolving);
			if (needs_resolving || OpStatus::IsError(OpenTrustInformation(is_local)))
				m_listener->OnTrustInfoResolveError();
			else
				m_listener->OnTrustInfoResolved();
		}
		OP_DELETE(socket_address);
	}
}

void TrustInformationResolver::OnHostResolverError(OpHostResolver* host_resolver, OpHostResolver::Error error)
{
	if (m_listener)
		m_listener->OnTrustInfoResolveError();
}

#endif // TRUST_INFO_RESOLVER
