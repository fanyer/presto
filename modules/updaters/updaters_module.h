/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_UDPATERS_MODULE_H
#define MODULES_UDPATERS_MODULE_H

#ifdef UPDATERS_ENABLE_MANAGER

class UpdatersModule : public OperaModule
{
private:
	unsigned int updater_counter;

public:
	/** Constructor */
	UpdatersModule():updater_counter(0){};

	/** Destructor */
	virtual ~UpdatersModule(){};

	/** Initialization */
	virtual void InitL(const OperaInitInfo& info){};

	/** Clean up the module object */
	virtual void Destroy(){};

	unsigned int NewUpdaterId(){return ++updater_counter;}

};

#define UPDATERS_MODULE_REQUIRED

#define g_updaters_api                          (&(g_opera->updaters_module))

#endif

#endif // MODULES_UDPATERS_MODULE_H
