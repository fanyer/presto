/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/upload/upload_module.h"

#include "modules/url/tools/arrays.h"

void
UploadModule::InitL(const OperaInitInfo& info)
{
#ifdef HAS_SET_HTTP_DATA
	CONST_ARRAY_INIT_L(headers_def);
	CONST_ARRAY_INIT_L(Upload_untrusted_headers_HTTP);
	CONST_ARRAY_INIT_L(Upload_untrusted_headers_Bcc);
	CONST_ARRAY_INIT_L(Upload_untrusted_headers_HTTPContentType);
#endif
}

void
UploadModule::Destroy()
{
#ifdef HAS_SET_HTTP_DATA
	CONST_ARRAY_SHUTDOWN(headers_def);
	CONST_ARRAY_SHUTDOWN(Upload_untrusted_headers_HTTP);
	CONST_ARRAY_SHUTDOWN(Upload_untrusted_headers_Bcc);
	CONST_ARRAY_SHUTDOWN(Upload_untrusted_headers_HTTPContentType);
#endif
}

