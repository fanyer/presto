/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef INPUTMANAGER_MODULE_H
#define INPUTMANAGER_MODULE_H

class OpInputManager;

class InputmanagerModule
	: public OperaModule
{
public:
	InputmanagerModule() 
		: m_input_manager(NULL)
	{ }

	virtual void InitL(const OperaInitInfo &info);
	virtual void Destroy();

	OpInputManager* m_input_manager;

#ifndef HAS_COMPLEX_GLOBALS
private:
	/* Duplicate OpInputAction's enum here to avoid having to include
	 * all of inputaction.h */
	#include "modules/hardcore/actions/generated_actions_enum.h"
public:
	const char* s_action_strings[LAST_ACTION + 1];
#endif // HAS_COMPLEX_GLOBALS
};

#ifndef HAS_COMPLEX_GLOBALS
#define s_action_strings g_opera->inputmanager_module.s_action_strings
#endif // HAS_COMPLEX_GLOBALS

#define INPUTMANAGER_MODULE_REQUIRED

#endif // INPUTMANAGER_CAPABILITIES_H
