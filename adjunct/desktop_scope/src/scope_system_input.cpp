/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/scope/src/scope_transport.h"
#include "adjunct/desktop_scope/src/scope_system_input.h"

#include "adjunct/desktop_pi/system_input_pi.h"

///////////////////////////////////////////////////////////////////////////////////////////////////

/* OpScopeSystemInput */

OpScopeSystemInput::OpScopeSystemInput()
	: m_system_input_pi(0)
{
	// Create the system input object
	OpStatus::Ignore(SystemInputPI::Create(&m_system_input_pi));
}

/* virtual */
OpScopeSystemInput::~OpScopeSystemInput()
{
	OP_DELETE(m_system_input_pi);
}

/* virtual */ OP_STATUS
OpScopeSystemInput::OnServiceEnabled()
{
	// Init the System Input when scope actually connects
	if (m_system_input_pi)
	{
		OpStatus::Ignore(m_system_input_pi->Init());
	}

	return OpStatus::OK;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

MouseButton OpScopeSystemInput::GetOperaButton(OpScopeSystemInput_SI::MouseInfo::MouseButton button)
{
	switch (button)
	{
		case MouseInfo::MOUSEBUTTON_LEFT:
			return MOUSE_BUTTON_1;

		case MouseInfo::MOUSEBUTTON_RIGHT:
			return MOUSE_BUTTON_2;

		case MouseInfo::MOUSEBUTTON_MIDDLE:
			return MOUSE_BUTTON_3;
			
		default:
			OP_ASSERT(!"Mouse button not supported");
			break;
	}
	
	return MOUSE_BUTTON_1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

ShiftKeyState OpScopeSystemInput::GetOperaModifier(UINT32 modifiers)
{
	ShiftKeyState opera_modifiers = 0;

	if ((modifiers & MODIFIERPRESSED_CTRL) == MODIFIERPRESSED_CTRL)
		opera_modifiers |= SHIFTKEY_CTRL;
	if ((modifiers & MODIFIERPRESSED_SHIFT) == MODIFIERPRESSED_SHIFT)
		opera_modifiers |= SHIFTKEY_SHIFT;
	if ((modifiers & MODIFIERPRESSED_ALT) == MODIFIERPRESSED_ALT)
		opera_modifiers |= SHIFTKEY_ALT;
	if ((modifiers & MODIFIERPRESSED_META) == MODIFIERPRESSED_META)
		opera_modifiers |= SHIFTKEY_META;
	if ((modifiers & MODIFIERPRESSED_SUPER) == MODIFIERPRESSED_SUPER)
		opera_modifiers |= SHIFTKEY_SUPER;
	
	return opera_modifiers;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS OpScopeSystemInput::DoClick(const MouseInfo &in)
{
	if (m_system_input_pi)
		m_system_input_pi->Click(in.GetX(), in.GetY(), GetOperaButton(in.GetButton()), in.GetNumClicks(), GetOperaModifier(in.GetModifier()));
	else
		return SetCommandError(OpScopeTPHeader::InternalError, UNI_L("System input service could not be initialized"));

	
	return OpStatus::OK;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS OpScopeSystemInput::DoKeyPress(const KeyPressInfo &in)
{
	// Convert the string e.g. "Control", "escape" to the OP_KEY
	const uni_char* key_string = in.GetKey().CStr();

	if (!key_string || !*key_string)
		return SetCommandError(OpScopeTPHeader::InternalError, UNI_L("KeyPress misses information about key"));;

	if (m_system_input_pi)
	{
		if (uni_strlen(key_string) > 1)
		{
			OpVirtualKey key = OpKey::FromString(key_string);
			m_system_input_pi->PressKey(key, key, GetOperaModifier(in.GetModifier()));
		}
		else
		{
			uni_char key = key_string[0];
			m_system_input_pi->PressKey(key, OP_KEY_INVALID, GetOperaModifier(in.GetModifier()));
		}
	}
	else 
	{
		return SetCommandError(OpScopeTPHeader::InternalError, UNI_L("System input service could not be initialized"));
	}
	
	return OpStatus::OK;
}


OP_STATUS OpScopeSystemInput::DoKeyUp(const KeyPressInfo &in)
{
	// Convert the string e.g. "Control", "escape" to the OP_KEY
	const uni_char* key_string = in.GetKey().CStr();

	if (!key_string || !*key_string)
		return SetCommandError(OpScopeTPHeader::InternalError, UNI_L("KeyUp misses information about key"));;
	if (!m_system_input_pi)
		return SetCommandError(OpScopeTPHeader::InternalError, UNI_L("System input service could not be initialized"));

	if (uni_strlen(key_string) > 1)
	{
		OpVirtualKey key = OpKey::FromString(key_string);
		m_system_input_pi->KeyUp(key, key, GetOperaModifier(in.GetModifier()));
	}
	else
	{
		uni_char key = key_string[0];
		m_system_input_pi->KeyUp(key, OP_KEY_INVALID, GetOperaModifier(in.GetModifier()));
	}
	
	return OpStatus::OK;
}

OP_STATUS OpScopeSystemInput::DoKeyDown(const KeyPressInfo &in)
{
	// Convert the string e.g. "Control", "escape" to the OP_KEY
	const uni_char* key_string = in.GetKey().CStr();

	if (!key_string || !*key_string)
		return SetCommandError(OpScopeTPHeader::InternalError, UNI_L("KeyDown misses information about key"));;
	if (!m_system_input_pi)
		return SetCommandError(OpScopeTPHeader::InternalError, UNI_L("System input service could not be initialized"));

	if (uni_strlen(key_string) > 1)
	{
		OpVirtualKey key = OpKey::FromString(key_string);
		m_system_input_pi->KeyDown(key, key, GetOperaModifier(in.GetModifier()));
	}
	else
	{
		uni_char key = key_string[0];
		m_system_input_pi->KeyDown(key, OP_KEY_INVALID, GetOperaModifier(in.GetModifier()));
	}
	
	return OpStatus::OK;
}

OP_STATUS OpScopeSystemInput::DoMouseDown(const MouseInfo &in)
{
	if (m_system_input_pi)
		m_system_input_pi->MouseDown(in.GetX(), in.GetY(), GetOperaButton(in.GetButton()), GetOperaModifier(in.GetModifier()));
	else
		return SetCommandError(OpScopeTPHeader::InternalError, UNI_L("System input service could not be initialized"));

	return OpStatus::OK;
}

OP_STATUS OpScopeSystemInput::DoMouseUp(const MouseInfo &in)
{
	if (m_system_input_pi)
		m_system_input_pi->MouseUp(in.GetX(), in.GetY(), GetOperaButton(in.GetButton()), GetOperaModifier(in.GetModifier()));
	else
		return SetCommandError(OpScopeTPHeader::InternalError, UNI_L("System input service could not be initialized"));

	return OpStatus::OK;
}

OP_STATUS OpScopeSystemInput::DoMouseMove(const MouseInfo &in)
{
	if (m_system_input_pi)
		m_system_input_pi->MouseMove(in.GetX(), in.GetY(), GetOperaButton(in.GetButton()), GetOperaModifier(in.GetModifier()));
	else
		return SetCommandError(OpScopeTPHeader::InternalError, UNI_L("System input service could not be initialized"));

	return OpStatus::OK;
}

