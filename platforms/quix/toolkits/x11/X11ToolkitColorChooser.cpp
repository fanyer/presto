/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Espen Sand
 */

#include "core/pch.h"

#include "X11ToolkitColorChooser.h"

#include "platforms/quix/dialogs/ColorSelectorDialog.h"
#include "platforms/unix/base/x11/x11utils.h"


X11ToolkitColorChooser::X11ToolkitColorChooser()
	:m_accepted(false)
	,m_color(0)
{
}


X11ToolkitColorChooser::~X11ToolkitColorChooser()
{
}

bool X11ToolkitColorChooser::Show(X11Types::Window parent, uint32_t initial_color)
{
	m_color = initial_color;

	ColorSelectorDialog* dialog = OP_NEW(ColorSelectorDialog,(initial_color));
	if (!dialog)
		return false;

	dialog->SetColorSelectorDialogListener(this);
	dialog->Init(X11Utils::GetDesktopWindowFromX11Window(parent));

	return m_accepted;
}

uint32_t X11ToolkitColorChooser::GetColor()
{
	return m_color;
}
