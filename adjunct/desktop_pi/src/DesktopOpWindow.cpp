/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#include "adjunct/desktop_pi/DesktopOpWindow.h"


OP_STATUS OpWindow::Create(OpWindow** new_opwindow)
{
	DesktopOpWindow* desktop_op_window;
	OP_STATUS		 ret = DesktopOpWindow::Create(&desktop_op_window);

	*new_opwindow = desktop_op_window;

	return ret;
}
