/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef M2_CAPABILITIES_H
#define M2_CAPABILITIES_H

/* MessageEngine::LoadRSS has an extra URL-type parameter */
#define M2_CAP_LOAD_RSS_FROM_URL

/* M2 knows about 'low bandwidth mode' */
#define M2_CAP_LOW_BANDWIDTH_MODE

/* M2 has AccountManager::SetLowBandwidthMode and AccountManager::IsLowBandwidthModeEnabled */
#define M2_CAP_ACCOUNTMGR_LOW_BANDWIDTH_MODE

/* M2 uses the webfeeds module for parsing */
#define M2_CAP_USES_WEBFEEDS_BACKEND

/* M2 has Index::MessageNotVisible and Engine::SetUserWantsToDoRecovery and similar */
#define M2_CAP_HAS_ENGINE_RECOVERY_AND_INDEX_NOT_VISIBLE

#endif // M2_CAPABILITIES_H
