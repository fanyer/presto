/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */
#include "core/pch.h"

#include "adjunct/quick/controller/NameCompletionController.h"

#include "adjunct/quick_toolkit/bindings/QuickBinder.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"

void NameCompletionController::InitL()
{
	LEAVE_IF_ERROR(SetDialog("Name Completion Dialog"));
	LEAVE_IF_ERROR(InitOptions());
}

OP_STATUS NameCompletionController::InitOptions()
{
	RETURN_IF_ERROR(GetBinder()->Connect("Prefixes_edit", *g_pcnet, PrefsCollectionNetwork::HostNameExpansionPrefix));
	RETURN_IF_ERROR(GetBinder()->Connect("Suffixes_edit", *g_pcnet, PrefsCollectionNetwork::HostNameExpansionPostfix));
	RETURN_IF_ERROR(GetBinder()->Connect("Completion_checkbox", *g_pcnet, PrefsCollectionNetwork::CheckPermutedHostNames));
	RETURN_IF_ERROR(GetBinder()->Connect("Local_machine_checkbox", *g_pcnet, PrefsCollectionNetwork::CheckHostOnLocalNetwork));

	return OpStatus::OK;
}

