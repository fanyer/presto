/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef __MACSHAREDMEMORYCREATOR_H__
#define __MACSHAREDMEMORYCREATOR_H__

#include "platforms/mac/util/MacSharedMemory.h"

class MacSharedMemoryCreator : public MacSharedMemory
{
public:
	MacSharedMemoryCreator();
	virtual ~MacSharedMemoryCreator();

	OP_STATUS	Create(int size);
	void		Destroy();
	
private:
	static int s_current_key;
};

#endif // __MACSHAREDMEMORYCREATOR_H__
