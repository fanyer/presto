/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Blazej Kazmierczak (bkazmierczak)
 */


#ifndef EXTENSION_UPDATER_H
#define EXTENSION_UPDATER_H

#include "adjunct/autoupdate_gadgets/GadgetUpdateController.h"


/**
 * Class responsible for handling auto update of exensions.
 */
class ExtensionUpdater : 
		public GadgetUpdateListener
{

public:
	ExtensionUpdater(GadgetUpdateController& controller);

	virtual ~ExtensionUpdater();

	//=================GadgetUpdateListener=============================
	virtual void OnGadgetUpdateFinish(GadgetUpdateInfo* data, GadgetUpdateController::GadgetUpdateResult result);
	virtual void OnGadgetUpdateDownloaded(GadgetUpdateInfo* data,OpStringC& update_source);
	virtual void OnGadgetUpdateAvailable(GadgetUpdateInfo* data,BOOL visible);

private:
	OP_STATUS CreateUpdateUrlFile(GadgetUpdateInfo* data);

	GadgetUpdateInfo*						m_update_data;
	GadgetUpdateController*					m_controller;
};

#endif // EXTENSION_UPDATER_H
