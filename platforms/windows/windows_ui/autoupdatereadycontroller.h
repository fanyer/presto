/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef AUTOUPDATE_READY_CONTROLLER_H
#define AUTOUPDATE_READY_CONTROLLER_H

#ifdef AUTO_UPDATE_SUPPORT
#ifdef AUTOUPDATE_PACKAGE_INSTALLATION

#include "adjunct/quick/controller/SimpleDialogController.h"

class AutoUpdateReadyController : public SimpleDialogController
{
public:
	AutoUpdateReadyController(BOOL needs_elevation);

private:
	virtual void InitL();
	virtual void OnCancel();

	BOOL m_needs_elevation;
};

#endif // AUTOUPDATE_PACKAGE_INSTALLATION
#endif // AUTO_UPDATE_SUPPORT

#endif // AUTOUPDATE_READY_CONTROLLER_H
