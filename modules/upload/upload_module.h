/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_UPLOAD_MODULE_H
#define MODULES_UPLOAD_MODULE_H

#include "modules/url/tools/arrays_decl.h"

class UploadModule : public OperaModule
{
public:
#ifdef HAS_SET_HTTP_DATA
	DECLARE_MODULE_CONST_ARRAY(const char*, headers_def);
	DECLARE_MODULE_CONST_ARRAY(KeywordIndex, Upload_untrusted_headers_HTTP);
	DECLARE_MODULE_CONST_ARRAY(KeywordIndex, Upload_untrusted_headers_Bcc);
	DECLARE_MODULE_CONST_ARRAY(KeywordIndex, Upload_untrusted_headers_HTTPContentType);
#endif
public:
	UploadModule(){};
	virtual ~UploadModule(){};

	virtual void InitL(const OperaInitInfo& info);
	virtual void Destroy();
};

#define UPLOAD_MODULE_REQUIRED
#define g_upload_module g_opera->upload_module

#ifndef HAS_COMPLEX_GLOBALS
#ifdef HAS_SET_HTTP_DATA
#define g_headers_def	CONST_ARRAY_GLOBAL_NAME(upload, headers_def)
#define g_Upload_untrusted_headers_HTTP	CONST_ARRAY_GLOBAL_NAME(upload, Upload_untrusted_headers_HTTP)
#define g_Upload_untrusted_headers_Bcc	CONST_ARRAY_GLOBAL_NAME(upload, Upload_untrusted_headers_Bcc)
#define g_Upload_untrusted_headers_HTTPContentType	CONST_ARRAY_GLOBAL_NAME(upload, Upload_untrusted_headers_HTTPContentType)
#endif
#endif

#endif // !MODULES_UPLOAD_MODULE_H
