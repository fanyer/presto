/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "platforms/mac/pi/MacOpDragObject.h"
#include "modules/pi/OpSystemInfo.h"

/* static */
OP_STATUS OpDragObject::Create(OpDragObject*& object, OpTypedObject::Type type)
{
	RETURN_OOM_IF_NULL(object = OP_NEW(MacOpDragObject, (type)));
	return OpStatus::OK;
}

MacOpDragObject::~MacOpDragObject()
{
}

MacOpDragObject::MacOpDragObject(OpTypedObject::Type type) : DesktopDragObject(type)
{
}

DropType MacOpDragObject::GetSuggestedDropType() const
{
	ShiftKeyState modifiers = g_op_system_info->GetShiftKeyState();
	if ((modifiers & ~SHIFTKEY_SHIFT) == 0)
		return DesktopDragObject::GetSuggestedDropType();

//	if (modifiers & SHIFTKEY_COMMAND)  // This would be "correct" but GetShiftKeyState returns Ctrl when Cmd is pressed.
	if (modifiers & SHIFTKEY_CTRL)
	{
		if (modifiers & SHIFTKEY_ALT)
			return DROP_LINK;
		return DROP_MOVE;
	}
	return DROP_COPY;
}
