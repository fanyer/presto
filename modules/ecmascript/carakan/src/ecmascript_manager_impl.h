/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  1995-2005
 *
 * Concrete implementation of EcmaScript_Manager interface
 * Lars Thomas Hansen
 */

#ifndef ECMASCRIPT_MANAGER_IMPL_H
#define ECMASCRIPT_MANAGER_IMPL_H

/*******************************************************************************
 *
 * EcmaScript_Manager_Impl definition
 */

class EcmaScript_Manager_Impl : public EcmaScript_Manager
{
public:
	EcmaScript_Manager_Impl();

	virtual ~EcmaScript_Manager_Impl();

#ifndef _STANDALONE
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
#endif // _STANDALONE

	static OP_STATUS Initialise();
	    /**< Load and initialize the engine if it has not already been loaded.
			 (If you're running DLL-less, then loading is trivial.)

	         @return  OpStatus::OK if initialization went OK and the DLL is
		              ready for use; some error code otherwise.
	         */
};

#endif // ECMASCRIPT_MANAGER_IMPL_H
