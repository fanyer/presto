/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef SPEED_DIAL_EXTENSION_PREF_H
#define SPEED_DIAL_EXTENSION_PREF_H

#include "adjunct/quick_toolkit/contexts/CloseDialogContext.h"

class OpWindowCommander;
class SpeedDialThumbnail;

/**
 * Controller for the Speed Dial Extension preferences dialog.
 */
class ExtensionPrefsController : public CloseDialogContext
{
public:
	explicit ExtensionPrefsController(SpeedDialThumbnail& thumbnail);

	OP_STATUS LoadPrefs(OpWindowCommander* commander, const OpStringC& url);

	/**
	 * @return the thumbnail currently associated with the controller.
	 */
	SpeedDialThumbnail* GetThumbnail() const { return m_thumbnail; }

private:
	// DialogContext
	virtual void InitL();

	SpeedDialThumbnail* m_thumbnail;
};

#endif // SPEED_DIAL_EXTENSION_PREF_H
