/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef BROWSER_UPGRADE_H
#define BROWSER_UPGRADE_H

#include "adjunct/desktop_util/version/operaversion.h"

/**
 * The place to put settings upgrade code in.
 */
class BrowserUpgrade
{
public:
	/**
	 * Upgrade existing settings/.ini files.
	 *
	 * Should be called only once (the first time opera is started after an upgrade).
	 *
	 * @param previous_version version of Opera that was used before the upgrade
	 */
	static OP_STATUS UpgradeFrom(OperaVersion previous_version);

private:
	static OP_STATUS UpgradeBrowserSettings(OperaVersion previous_version);
	static PrefsFile* GetToolbarSetupFile();

	static OP_STATUS To_12_50();
	static OP_STATUS To_12_00();
	static OP_STATUS To_11_60();
	static OP_STATUS To_11_51();
	static OP_STATUS To_11_50();
	static OP_STATUS To_11_10();
	static OP_STATUS To_11_01();
	static OP_STATUS To_11_00();
	static OP_STATUS To_10_70();
	static OP_STATUS To_10_61();
	static OP_STATUS To_10_60();
	static OP_STATUS To_10_50(); // 10.50 upgrading functionality didn't make it into 10.50 final
	static OP_STATUS To_10_10();
	static OP_STATUS To_9_60();
	static OP_STATUS To_9_50();
	static OP_STATUS To_9_20();
};

#endif // BROWSER_UPGRADE_H
