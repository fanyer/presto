/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/** \file
 * \brief Declare the global opera object for the memory module
 *
 * Various global variables used by the memory module goes into the
 * global opera object.
 *
 * \author Morten Rolland, moretnro@opera.com
 */

#ifndef MEMORY_MODULE_H
#define MEMORY_MODULE_H

#define MEMORY_MODULE_REQUIRED

#define MPL_MAX_POOLS 2560


/**
 * \brief Take care of initialization and global variables.
 *
 * All global variables are located in the global opera object, with
 * MemoryModule as the container for the memory module. All
 * variables are prefixed with mpl_ in this class, but must be
 * referenced through the corresponding macros with g_ prepended.
 */
class MemoryModule : public OperaModule
{
public:
	/**
	 * \brief Initialize MemoryModule for use.
	 *
	 * Since this module is used by many of the other modules, the
	 * core initialization of this module is done in its constructor.
	 * This will assure this module to be initialized when the first
	 * InitL() method is called on any opera object.
	 */
	MemoryModule(void);

	/**
	 * \brief Free up resources in MemoryModule.
	 *
	 * We clean up and free all the resources this module has been
	 * using only at destructor time; this will assure all other
	 * modules can depend on this module until the very end.
	 */
	~MemoryModule(void);

	/**
	 * \brief Opera Object Initialization method.
	 *
	 * This method does not do anything, as the main initialization is
	 * done in the constructor.
	 */
	void InitL(const OperaInitInfo& info);

	/**
	 * \brief Opera Object Destroy method.
	 *
	 * This method does not do anything, as termination is done in the
	 * constructor.
	 */
	void Destroy(void);
};

#endif // MEMORY_MODULE_H

#include "modules/memory/src/memory_state.h"
