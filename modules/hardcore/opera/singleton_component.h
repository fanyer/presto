/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef OPERA_SINGLETON_COMPONENT_H
#define OPERA_SINGLETON_COMPONENT_H

/**
 * Singleton component referred to by modules/hardcore/module.components.
 *
 * Trivial instance of CoreComponent running all of Core inside one single component.
 */
class SingletonComponent
	: public CoreComponent
{
public:
	SingletonComponent(const OpMessageAddress& address)
		: CoreComponent(address, COMPONENT_SINGLETON) {}
};

#endif // OPERA_SINGLETON_COMPONENT_H
