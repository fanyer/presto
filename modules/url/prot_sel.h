/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2002-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef PROT_SEL_H
#define PROT_SEL_H

#if defined(URL_USE_SCHEME_ALIAS_WSP)
# define URL_USE_SCHEME_ALIAS
#endif

struct protocol_selentry
{
	const char *protocolname;
	const uni_char *uni_protocolname;
	URLType real_urltype;
	URLType used_urltype;

	BOOL	have_servername;
	unsigned short default_port;
#ifdef EXTERNAL_PROTOCOL_SCHEME_SUPPORT
	BOOL	is_external;
#endif
#ifdef URL_USE_SCHEME_ALIAS
	const char *alias_scheme;
#endif

};

#endif // !PROT_SEL_H
