/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef DESKTOP_OP_VIEW_H
#define DESKTOP_OP_VIEW_H

class OpView;
class OpWindow;

/** @brief Desktop specific handling of OpView pi
 *
 * Desktop platform implementations of OpView class should use Create methods of
 * this class instead of OpView::Create.
 *
 * @see DesktopOpView.cpp
 */
class DesktopOpView
{
	DesktopOpView() {}

public:

	static OP_STATUS Create(OpView** new_opview, OpWindow* parentwindow);

	static OP_STATUS Create(OpView** new_opview, OpView* parentview);
};

#endif // DESKTOP_OP_VIEW_H
