/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Espen Sand
 */

#ifndef X11_TOOLKIT_COLOR_CHOOSER_H
#define X11_TOOLKIT_COLOR_CHOOSER_H

#include "platforms/quix/toolkits/ToolkitColorChooser.h"
#include "platforms/quix/dialogs/ColorSelectorDialog.h"

class X11ToolkitColorChooser : public ToolkitColorChooser, public ColorSelectorDialog::ColorSelectorDialogListener
{
public:
	X11ToolkitColorChooser();
	~X11ToolkitColorChooser();
	bool Show(X11Types::Window parent, uint32_t initial_color);
	uint32_t GetColor();

	// ColorSelectorDialog::ColorSelectorDialogListener
	void OnDialogClose(bool accepted, UINT32 color) { m_accepted = accepted; m_color = color; }

private:
	bool m_accepted;
	uint32_t m_color;
};

#endif
