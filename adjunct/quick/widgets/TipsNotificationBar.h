/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */
#ifndef TIPS_NOTIFICATIONBAR_H
#define TIPS_NOTIFICATIONBAR_H

class OpWidget;

#include "adjunct/quick_toolkit/widgets/OpToolbar.h"

class TipsNotificationBar
	: public OpToolbar
{

protected:
	TipsNotificationBar();
	virtual ~TipsNotificationBar();

public:
	static OP_STATUS Construct(TipsNotificationBar** obj, OpWidget *parent);

	OP_STATUS SetTips(const OpString &tip);
	OP_STATUS SetInfoImage(const char *image_name);

	//OpToolbar
	virtual void OnSettingsChanged(DesktopSettings* settings) { }
	virtual BOOL OnInputAction(OpInputAction* action);
};

#endif // TIPS_NOTIFICATIONBAR_H
