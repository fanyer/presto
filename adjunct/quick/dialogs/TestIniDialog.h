/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "adjunct/quick_toolkit/widgets/Dialog.h"

/** @brief A dummy dialog class that can be used to test dialog layouts
  * This is normally activated via the -inidialogtest "Dialog To Test" command line argument.
  */
class TestIniDialog : public Dialog
{
public:
	virtual ~TestIniDialog() { g_application->Exit(TRUE); }

	OP_STATUS SetDialogName(const OpStringC& name) { return m_dialog_name.Set(name); }

	virtual const char* GetWindowName() { return m_dialog_name.CStr(); }

private:
	OpString8 m_dialog_name;
};
