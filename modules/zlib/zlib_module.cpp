/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#if defined(USE_ZLIB) && !defined(USE_SYSTEM_ZLIB)
#ifdef SUPPORT_DEFLATE

#include "modules/zlib/zlib_module.h"
#include "modules/zlib/zlib_treedesc.h"
extern "C" {
#include "modules/zlib/deflate.h"
}
#include "modules/zlib/deflate_config.h"

ZlibModule::ZlibModule()
	: m_deflate_config(0)
{
	InitZLibTreeDescs(&m_static_l_desc, &m_static_d_desc, &m_static_bl_desc);
}

void ZlibModule::InitL(const OperaInitInfo& info)
{
#ifdef FASTEST
	m_deflate_config = OP_NEWA(config, 2);
#else // FASTEST
	m_deflate_config = OP_NEWA(config, 10);
#endif // FASTEST
	LEAVE_IF_NULL(m_deflate_config);
	InitZLibDeflateConfigurationTable(m_deflate_config);
}

void ZlibModule::Destroy()
{
	OP_DELETEA(m_deflate_config);
	m_deflate_config = 0;
}

/* extern "C" */
static_tree_desc_s* get_l_desc()
{
    return &(g_opera->zlib_module.m_static_l_desc);
}

/* extern "C" */
static_tree_desc_s* get_d_desc()
{
    return &(g_opera->zlib_module.m_static_d_desc);
}

/* extern "C" */
static_tree_desc_s* get_bl_desc()
{
    return &(g_opera->zlib_module.m_static_bl_desc);
}

/* extern "C" */
config_s* get_deflate_config()
{
	OP_ASSERT(g_opera->zlib_module.m_deflate_config);
	return g_opera->zlib_module.m_deflate_config;
}

#endif // SUPPORT_DEFLATE
#endif // defined(USE_ZLIB) && !defined(USE_SYSTEM_ZLIB)
