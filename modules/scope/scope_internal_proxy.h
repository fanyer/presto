/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_SCOPE_SCOPE_INTERNAL_PROXY_H
#define MODULES_SCOPE_SCOPE_INTERNAL_PROXY_H

#if defined(SCOPE_SUPPORT) && defined(SCOPE_ACCEPT_CONNECTIONS)

/**
 * Public API for the internal proxy in the scope module.
 */
class OpScopeProxy
{
public:
#ifdef SCOPE_MESSAGE_TRANSCODING
	enum TranscodingFormat
	{
		  Format_None = 0
		, Format_ProtocolBuffer = 1
		, Format_JSON = 2
		, Format_XML = 3
	};

	/**
	 * Changes the current format used for transcoding in the proxy.
	 * Can be called at any time.
	 *
	 * @note Only available for testing purposes.
	 * @see OpScopeSocketHostConnection::InitiateTranscoding
	 */
	static OP_STATUS SetTranscodingFormat(TranscodingFormat format);

	static void GetTranscodingStats(unsigned &client_to_host, unsigned &host_to_client);
#endif // SCOPE_MESSAGE_TRANSCODING
};

#endif // SCOPE_SUPPORT && SCOPE_ACCEPT_CONNECTIONS

#endif // !MODULES_SCOPE_SCOPE_INTERNAL_PROXY_H
