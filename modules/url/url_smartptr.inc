/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "modules/url/url2.h"
#include "modules/url/url_sn.h"
#include "modules/url/url_scheme.h"
#include "modules/url/protocols/http1.h"
#include "modules/url/protocols/ftp.h"

template class OpSmartPointerNoDelete<ServerName>;

template class OpSmartPointerNoDelete<HTTP_Server_Manager>;
#ifndef NO_FTP_SUPPORT
template class OpSmartPointerNoDelete<FTP_Server_Manager>;
#endif // NO_FTP_SUPPORT

template class OpSmartPointerWithDelete<URL_Scheme_Authority_Components>;

template class OpSmartPointerWithDelete<HeaderInfo>;
