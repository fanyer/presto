/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef COCOA_APPLICATION_H
#define COCOA_APPLICATION_H

void ApplicationRun();
void ApplicationTerminate();
void ApplicationActivate();

class Pool
{
public:
	Pool();
	~Pool();
private:
	void* pool;
};

#endif // COCOA_APPLICATION_H
