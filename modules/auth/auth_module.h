/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_AUTH_URL_MODULE_H
#define MODULES_AUTH_URL_MODULE_H

#include "modules/url/tools/arrays_decl.h"

struct KeywordIndex;

class AuthModule : public OperaModule
{
public:
#ifdef HTTP_DIGEST_AUTH
	DECLARE_MODULE_CONST_ARRAY(KeywordIndex, digest_algs);
	DECLARE_MODULE_CONST_ARRAY(const char*, HTTP_method_texts);
#endif

public:
	AuthModule(){};
	virtual ~AuthModule(){};

	virtual void InitL(const OperaInitInfo& info);
	virtual void Destroy();
};

#ifndef HAS_COMPLEX_GLOBALS
#ifdef HTTP_DIGEST_AUTH
# define g_digest_algs	CONST_ARRAY_GLOBAL_NAME(auth, digest_algs)
# define g_HTTP_method_texts	CONST_ARRAY_GLOBAL_NAME(auth, HTTP_method_texts)
#endif
#endif


#define AUTH_MODULE_REQUIRED

#endif // !MODULES_AUTH_URL_MODULE_H
