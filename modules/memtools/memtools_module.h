/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/** \file
 * \brief Declare the global opera object for the memtools module
 *
 * Various global variables used by the memtools module goes into the
 * global opera object.
 *
 * \author Morten Rolland, mortenro@opera.com
 */


#ifndef MODULES_MEMTOOLS_MEMTOOLS_MODULE_H
#define MODULES_MEMTOOLS_MEMTOOLS_MODULE_H

#define MEMTOOLS_MODULE_REQUIRED

class MemtoolsModule : public OperaModule
{
public:
	virtual void InitL(const OperaInitInfo &info);
	virtual void Destroy(void);
};

#endif // !MODULES_MEMTOOLS_MEMTOOLS_MODULE_H
