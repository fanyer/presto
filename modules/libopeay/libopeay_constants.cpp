/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

// Inside wrapper. No need for default include files

#ifndef HAS_COMPLEX_GLOBALS

#include "modules/libopeay/libopeay_arrays.h"

void LibopeayModule::AddComplexConstGlobal(Libopeay_ConstBase *item)
{
	if(item)
		item->Into(&global_const_list);
}

void LibopeayModule::InitGlobalsL()
{
	m_consts = new (ELeave) Libopeay_GlobalConst_Data;

	if(global_const_list.Empty())
		return; // If nothing to do, don't bother

	// Alloc, if necessary
	InitGlobalsPhaseL(1);
	// Init members
	InitGlobalsPhaseL(2);
}

void LibopeayModule::InitGlobalsPhaseL(int phase)
{
	Libopeay_ConstBase *item = (Libopeay_ConstBase *) global_const_list.First();

	while(item)
	{
		item->InitL(phase);

		item = (Libopeay_ConstBase *) item->Suc();
	}
}

void LibopeayModule::DestroyGlobals()
{
	Libopeay_ConstBase *item = (Libopeay_ConstBase *) global_const_list.First();

	while(item)
	{
		item->Destroy();

		item = (Libopeay_ConstBase *) item->Suc();
	}

	delete m_consts;
	m_consts = NULL;
}

LibopeayModule::Libopeay_ConstBase::Libopeay_ConstBase()
{
	g_opera->libopeay_module.AddComplexConstGlobal(this);
}

LibopeayModule::Libopeay_ConstBase::~Libopeay_ConstBase()
{
	if(InList())
		Out();
}


#endif // HAS_COMPLEX_GLOBALS
