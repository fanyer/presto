/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef WAND_CAPABILITIES_H
#define WAND_CAPABILITIES_H

#define WAND_CAP_MAKEWANDURL

/** Has UnreferenceDocument */
#define WAND_CAP_UNREFERENCEDOCUMENT

/** Has MakeWandServerUrl */
#define WAND_CAP_MAKEWANDSERVERURL

/**
 * Has WandPage::FindUserName that returns an informative enum, and the old FindUserName is deprecated.
 */
#define WAND_CAP_FINDUSERNAME_WITH_ENUM_RETURN

/**
 * Wand can now let the submit go on while waiting for the user's decision.
 */
#define WAND_CAP_NON_BLOCKING_SUBMIT

/**
 * The new cleaned up API with asynchronous operations.
 */
#define WAND_CAP_GANDALF_3_API

/**
 * WandPage::FindUserName takes a FramesDocument pointer.
 */
#define WAND_CAP_FINDUSERNAME_TAKES_DOC

/**
 * WandManager::ClearAll takes a include_internal parameter, and DeleteLoginsWithIdPrefix exists
 */
#define WAND_CAP_DELETE_WITH_PREFIX

/**
 * WandManager::UpdateSecurityStateWithoutWindow() and WandManager::GetLongPasswordWithoutWindow(...) exist.
 */
#define WAND_CAP_OPS_WITHOUT_WINDOW

/**
 * WandManager::StoreLoginWithoutWindow exist.
 */
#define WAND_CAP_MORE_OPS_WITHOUT_WINDOW

/**
 * UpdateSecurityState now signals to the UI through
 * WandListener::OnSecurityStateChange how the operation
 * ended. Also, UpdateSecurityState no longer returns anything.
 */
#define WAND_CAP_ON_SECURITY_STATE_CHANGE

/**
 * Stopped using DOMCONTROLVALUECHANGED.
 */
#define WAND_CAP_NO_DOMCONTROLVALUECHANGED

#endif // !WAND_CAPABILITIES_H
