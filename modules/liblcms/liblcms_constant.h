/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_LIBLCMS_LIBLCMS_CONSTANT_H
#define MODULES_LIBLCMS_LIBLCMS_CONSTANT_H

// pre-defined macro for initialize global variables in liblcms

#ifdef HAS_COMPLEX_GLOBALS

#define LIBLCMS_CONST_ARRAY(name, type, size)	static type name[size] = { 
#define LIBLCMS_CONST_END(name)					};
#define LIBLCMS_CONST_INIT(name)				((void)0)

#else

#define LIBLCMS_GLOBAL_BEGIN(name)	void liblcms_init##name(void) {
#define LIBLCMS_GLOBAL_END(name)	}

#define LIBLCMS_CONST_ARRAY(name, type, size)	static type name[size]; \
												void liblcms_init##name(void) { \
													op_memset(name, 0, sizeof(name)); \
													type* item = name;
#define LIBLCMS_CONST_END(name)					}
#define LIBLCMS_CONST_INIT(name)				liblcms_init##name()

#endif // HAS_COMPLEX_GLOBALS

#endif // MODULES_LIBLCMS_LIBLCMS_CONSTANT_H
