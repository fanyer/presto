/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef AUTO_WINDOW_RELOAD_CONTROLLER_H
#define AUTO_WINDOW_RELOAD_CONTROLLER_H

#include "adjunct/desktop_util/adt/opproperty.h"
#include "adjunct/quick_toolkit/contexts/OkCancelDialogContext.h"

class OpWindowCommander;
class DesktopSpeedDial;

/**
 *
 *	AutoWindowReloadController - allows to set up custom reload time for web pages and thumbnails in SD
 *
 */
class AutoWindowReloadController : public OkCancelDialogContext
{
public:
	AutoWindowReloadController(OpWindowCommander* win_comm, const DesktopSpeedDial* sd = NULL);

private:
	virtual		void InitL();

	virtual void OnOk();
	BOOL		DisablesAction(OpInputAction* action);
	bool		VerifyInput();
	OP_STATUS	InitControls();
	void		OnDropDownChanged(const OpStringC& text);

	OpWindowCommander*			m_activewindow_commander;
	const DesktopSpeedDial*		m_sd;
	bool						m_verification_ok;
	OpProperty<OpString>		m_minutes;
	OpProperty<OpString>		m_seconds;
	OpProperty<bool>			m_reload_only_expired;
};


// Local function used in DocumentDesktopWindow and OpThumbnailWidget to control the
// selection of the "Custom..." menu item
BOOL IsStandardTimeoutMenuItem(int timeout);

#endif //AUTO_WINDOW_RELOAD_CONTROLLER_H
