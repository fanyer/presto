/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#if !defined UNICODE_MODULE_H
#define UNICODE_MODULE_H

#include "modules/hardcore/opera/module.h"

#if defined SUPPORT_TEXT_DIRECTION || !defined _NO_GLOBALS_
# define UNICODE_MODULE_REQUIRED
#endif

#ifdef UNICODE_MODULE_REQUIRED
class UnicodeModule : public OperaModule
{
public:
	UnicodeModule();
	virtual ~UnicodeModule() {}
	virtual void InitL(const OperaInitInfo &info);
	virtual void Destroy();

#ifdef SUPPORT_TEXT_DIRECTION
	int m_last_bidi_block;		///< Internal cache for bidi category code
	int m_last_bidi_plane;
	uni_char m_last_mirror[2];	///< Internal cache for bidi mirroring code /* ARRAY OK 2009-07-17 marcusc */
#endif
#ifndef _NO_GLOBALS_
	/** Character class cache entry */
	struct CC_CACHE_ENTRY {
		UnicodePoint c;
		CharacterClass cls;
	};
	/** Line breaking class cache entry */
	struct LBC_CACHE_ENTRY {
		UnicodePoint c;
		LinebreakClass lbc;
	};
	CC_CACHE_ENTRY m_cc_cache[2];
	int m_cc_cache_old;
	LBC_CACHE_ENTRY m_lbc_cache[2];
	int m_lbc_cache_old;
#endif // !_NO_GLOBALS_
};
#endif // UNICODE_MODULE_REQUIRED

#endif
