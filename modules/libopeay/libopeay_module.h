/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_LIBOPEAY_LIBOPEAY_MODULE_H
#define MODULES_LIBOPEAY_LIBOPEAY_MODULE_H

#if defined(_SSL_USE_OPENSSL_) || defined(LIBOPEAY_ENABLE_PARTLY_OPENSSL_SUPPORT)

#ifndef CLEAR_PASSWD_FROM_MEMORY
#error "For security reasons FEATURE_CLEAR_PASSWORDS *MUST* be YES when libssl with libopeay is used"
#endif

#include "modules/hardcore/opera/module.h"
#include "modules/util/simset.h"

class Libopeay_ImplementModuleData;
class Libopeay_GlobalConst_Data;

class LibopeayModule : public OperaModule
{
#ifndef HAS_COMPLEX_GLOBALS
private:
	Head global_const_list;

public:
	Libopeay_GlobalConst_Data *m_consts;
#endif

public:
	Libopeay_ImplementModuleData *m_data;

public:
	LibopeayModule();
	virtual void InitL(const OperaInitInfo& info);

	virtual void Destroy();
	virtual ~LibopeayModule();

#ifndef HAS_COMPLEX_GLOBALS
	class Libopeay_ConstBase : public Link
	{
	public:
		Libopeay_ConstBase();
		virtual ~Libopeay_ConstBase();
		virtual void InitL(int phase)=0;
		virtual void Destroy()=0;
	};

	void AddComplexConstGlobal(Libopeay_ConstBase *item);

private:
	void InitGlobalsL();
	void InitGlobalsPhaseL(int phase);
	void DestroyGlobals();

#endif
public:
#if !defined LIBOPEAY_DISABLE_MD5_PARTIAL
	BOOL turn_off_md5;
#endif
#if !defined LIBOPEAY_DISABLE_SHA1_PARTIAL
	BOOL turn_off_sha1;
#endif
};

#define LIBOPEAY_MODULE_REQUIRED

#define g_libopeay_module			g_opera->libopeay_module
#define g_turn_off_md5				g_libopeay_module.turn_off_md5
#define g_turn_off_sha1				g_libopeay_module.turn_off_sha1

#endif /* _SSL_USE_OPENSSL_ */

#endif /* !MODULES_LIBOPEAY_LIBOPEAY_MODULE_H */
