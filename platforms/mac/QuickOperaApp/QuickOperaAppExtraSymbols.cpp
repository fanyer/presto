/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Alexey Feldgendler
 */

#include "core/pch.h"

// This file includes a number of cpp files that are required by both
// OperaFramework and OperaApp. This workaround is necessary because
// the project file generation script currently cannot include the
// same file in two targets.

#include "adjunct/autoupdate/updater/audatafile_reader.cpp"
#include "adjunct/autoupdate/updater/austringutils.cpp"
#include "adjunct/autoupdate/updater/auupdater.cpp"
#include "adjunct/desktop_util/string/OpBasicString.cpp"
#include "adjunct/quick/managers/LaunchManager.cpp"
#include "platforms/mac/QuickOperaApp/QuickWidgetUnicodeUtils.cpp"
#include "platforms/mac/QuickOperaApp/macaufileutils.cpp"
#include "platforms/mac/pi/desktop/mac_launch_pi.cpp"
#include "adjunct/desktop_util/version/operaversion.cpp"