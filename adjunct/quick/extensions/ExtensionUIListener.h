/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef EXTENSION_UI_LISTENER_H
#define EXTENSION_UI_LISTENER_H

#include "modules/windowcommander/OpExtensionUIListener.h"

class OpGadget;
class OpWindowCommander;

class ExtensionDataListener
{
public:
	virtual ~ExtensionDataListener() {}

	virtual void OnExtensionDataUpdated(const OpStringC& title, const OpStringC& url) = 0;
};


class ExtensionUIListener:
	public OpExtensionUIListener
{
public:

	ExtensionUIListener() {}

	OP_STATUS Init() { return OpStatus::OK; }

	//
	// OpExtensionUIListener
	//

	virtual void OnExtensionSpeedDialInfoUpdated(OpWindowCommander* commander);

	virtual void OnExtensionUIItemAddRequest(
		OpWindowCommander* commander, 
		ExtensionUIItem* item);

	virtual void OnExtensionUIItemRemoveRequest(
		OpWindowCommander* commander,
		ExtensionUIItem* item);

private:

	OP_STATUS CreateExtensionButton(ExtensionUIItem* item, OpGadget *gadget);
	OP_STATUS UpdateExtensionButton(ExtensionUIItem* item);
	OP_STATUS DeleteExtensionButton(ExtensionUIItem* item);

	BOOL BlockedByPolicy(OpWindowCommander* commander, ExtensionUIItem* item);
};

#endif //EXTENSION_UI_LISTENER_H
