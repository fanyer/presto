/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_ZLIB_ZLIB_MODULE_H
#define MODULES_ZLIB_ZLIB_MODULE_H

#if defined(USE_ZLIB) && !defined(USE_SYSTEM_ZLIB)
#ifdef SUPPORT_DEFLATE

#include "modules/zlib/zlib_treedesc.h"
#include "modules/hardcore/opera/module.h"

struct config_s;

class ZlibModule : public OperaModule
{
public:
	ZlibModule();
	void InitL(const OperaInitInfo& info);
	void Destroy();

	static_tree_desc_s m_static_l_desc;
	static_tree_desc_s m_static_d_desc;
	static_tree_desc_s m_static_bl_desc;

	config_s* m_deflate_config;
};

#define ZLIB_MODULE_REQUIRED

#endif // SUPPORT_DEFLATE
#endif // defined(USE_ZLIB) && !defined(USE_SYSTEM_ZLIB)

#endif // !MODULES_ZLIB_ZLIB_MODULE_H
