/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_LIBOPEAY_ARRAYS_H
#define MODULES_LIBOPEAY_ARRAYS_H

#ifdef GLOBAL_CONST_WRAPPER_BASE
#undef GLOBAL_CONST_WRAPPER_BASE
#endif
#define GLOBAL_CONST_WRAPPER_BASE : public LibopeayModule::Libopeay_ConstBase

#include "modules/url/tools/arrays.h"
#include "modules/libopeay/libopeay_constants.h"

#ifdef HAS_COMPLEX_GLOBALS
#define OPENSSL_GLOBAL_ARRAY_NAME(name) g_##name
#define OPENSSL_GLOBAL_ITEM_NAME(name) g_##name
#define OPENSSL_CONST_ARRAY_SIZE(name) CONST_ARRAY_SIZE(libopeay, name)
#else
#define OPENSSL_GLOBAL_ARRAY_NAME(name) g_opera->libopeay_module.m_consts->m_cda_##name .m_array
#define OPENSSL_GLOBAL_ITEM_NAME(name)  g_opera->libopeay_module.m_consts->m_cdi_##name .m_item
#define OPENSSL_CONST_ARRAY_SIZE(name)  (g_opera->libopeay_module.m_consts->m_cda_##name .m_size)
// Add namedefines above
#endif // Complex globals

#define OPENSSL_GLOBAL_NAME_it(name)		OPENSSL_GLOBAL_ITEM_NAME(name##_it) 
#define OPENSSL_GLOBAL_NAME_item_tt(name)	OPENSSL_GLOBAL_ITEM_NAME(name##_item_tt)
#define OPENSSL_GLOBAL_NAME_tt(name)		OPENSSL_GLOBAL_ITEM_NAME(name##_tt)
#define OPENSSL_GLOBAL_NAME_ch_tt(name)		OPENSSL_GLOBAL_ARRAY_NAME(name##_ch_tt)
#define OPENSSL_GLOBAL_NAME_seq_tt(name)	OPENSSL_GLOBAL_ARRAY_NAME(name##_seq_tt)
#define OPENSSL_GLOBAL_NAME_adbtbl(name)	OPENSSL_GLOBAL_ARRAY_NAME(name##_adbtbl)
#define OPENSSL_GLOBAL_NAME_adb(name)		OPENSSL_GLOBAL_ITEM_NAME(name##_adb)
#define OPENSSL_GLOBAL_NAME_aux(name)		OPENSSL_GLOBAL_ITEM_NAME(name##_aux)

#define OPENSSL_PREFIX_CONST_ARRAY(prefix, structname, name, modulename)	PREFIX_CONST_ARRAY(prefix, structname, name, modulename##_CONST)
#define OPENSSL_CONST_ARRAY(prefix, structname, name, modulename)			CONST_ARRAY(prefix, structname, name, modulename##_CONST)
#define OPENSSL_PREFIX_CONST_ITEM(prefix, structname, name, modulename)		PREFIX_CONST_ITEM(prefix, structname, name, modulename##_CONST)

#endif /* MODULES_LIBOPEAY_ARRAYS_H */
