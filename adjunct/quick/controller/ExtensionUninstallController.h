/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef EXTENSION_UNINSTALL_CONTROLLER_H
#define EXTENSION_UNINSTALL_CONTROLLER_H

#include "adjunct/desktop_util/adt/opproperty.h"
#include "adjunct/quick_toolkit/contexts/OkCancelDialogContext.h"

class OpGadgetClass;

/**
 *
 *	ExtensionUninstallController
 *
 *  Class controlls dialog which shows up if the user requested to uninstall extension.
 */
class ExtensionUninstallController : public OkCancelDialogContext
{
public:
	ExtensionUninstallController(OpGadgetClass& gclass, const uni_char* extension_id);

private:
	virtual void InitL();
	virtual void OnOk();

	const OpGadgetClass*	m_gclass;
	const uni_char*			m_extension_id;
	Image					m_icon_img;
};

#endif //EXTENSION_UNINSTALL_CONTROLLER_H
