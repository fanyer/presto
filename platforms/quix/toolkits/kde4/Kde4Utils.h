/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef KDE4_UTILS_H
#define KDE4_UTILS_H

#include "platforms/utilix/x11types.h"

class QDialog;
class QWidget;

namespace Kde4Utils
{
	/** Run a dialog and get its response code
	  * @param dialog Dialog to run
	  * @param parent Window ID of parent window
	  * @return Response code from dialog once closed
	  */
	int RunDialog(QDialog* dialog, X11Types::Window parent);

	/** Set widget resource name as reported by WM_CLASS property
	 *
	 * @param widget Should be a toplevel widget
	 * @param name Use lower case and no spacing (example "colorselectordialog")
	 */
	void SetResourceName(QWidget* widget, const char* name);
};

#endif // KDE4_UTILS_H
