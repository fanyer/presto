/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef SYSTEM_INPUT_DESKTOP_H
#define SYSTEM_INPUT_DESKTOP_H

#include "modules/hardcore/keys/opkeys.h"
#include "modules/scope/src/scope_service.h"
#include "adjunct/desktop_scope/src/generated/g_scope_system_input_interface.h"

class SystemInputPI;

class OpScopeSystemInput
	: public OpScopeSystemInput_SI
{
public:

	OpScopeSystemInput();
	virtual ~OpScopeSystemInput();

	// OpScopeService
	virtual OP_STATUS OnServiceEnabled();

	// Request/Response functions
	OP_STATUS DoClick(const MouseInfo &in);
	OP_STATUS DoKeyPress(const KeyPressInfo &in);
	OP_STATUS DoKeyUp(const KeyPressInfo &in);
	OP_STATUS DoKeyDown(const KeyPressInfo &in);
	OP_STATUS DoMouseDown(const MouseInfo &in);
	OP_STATUS DoMouseUp(const MouseInfo &in);
	OP_STATUS DoMouseMove(const MouseInfo &in);

private:
	SystemInputPI *m_system_input_pi;

	// Convert from SystemInfo message input enums to Opera enums
	MouseButton		GetOperaButton(OpScopeSystemInput_SI::MouseInfo::MouseButton button);
	ShiftKeyState	GetOperaModifier(UINT32 modifier);
};

#endif // SYSTEM_INPUT_DESKTOP_H
