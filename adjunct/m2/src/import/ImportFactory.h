/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 * 
 * George Refseth, rfz@opera.com
 */

#ifndef IMPORTFACTORY_H
#define IMPORTFACTORY_H

#include "adjunct/m2/src/include/defs.h"

class Importer;

class ImportFactory
{
public:
	OP_STATUS Create(EngineTypes::ImporterId id, Importer** importer);

	EngineTypes::ImporterId GetDefaultImporterId();	// returns default importer based on default mail client on system

	static ImportFactory* Instance();	// Singleton accessor, returns a pointer to the sole instance

protected:
	ImportFactory();	// protected constructor to prevent local instances
};

#endif//IMPORTFACTORY_H
