/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 */

#include "core/pch.h"

#include "adjunct/desktop_util/gpu/gpubenchmark.h"
#include "modules/prefsfile/prefsfile.h"
#include "modules/libvega/vegarenderer.h"
#include "modules/libvega/src/vegabackend_hw3d.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"

BOOL3 GetBackendStatus(OpStringC identifier)
{
	if (g_pccore->GetIntegerPref(PrefsCollectionCore::EnableHardwareAcceleration) == PrefsCollectionCore::Force)
		return YES;

	PrefsFile prefs(PREFS_INI);
	OpFile file;
	RETURN_VALUE_IF_ERROR(file.Construct(UNI_L("gpu_test.ini"), OPFILE_LOCAL_HOME_FOLDER), NO);
	
	BOOL exist = false;
	RETURN_VALUE_IF_ERROR(file.Exists(exist), NO);

	if (!exist)
		return MAYBE;

	BOOL3 ret = NO;
	TRAPD(err, 
		prefs.ConstructL();
		prefs.SetFileL(&file);
		RETURN_VALUE_IF_ERROR(prefs.LoadAllL(), NO);
		ret = (BOOL3)prefs.ReadIntL(identifier, UNI_L("Active"), MAYBE);
	);

	return ret;
}

OP_STATUS WriteBackendStatus(OpString identifier, bool enabled)
{
	PrefsFile prefs(PREFS_INI);
	OpFile file;
	RETURN_IF_ERROR(file.Construct(UNI_L("gpu_test.ini"), OPFILE_LOCAL_HOME_FOLDER));
	
	TRAP_AND_RETURN(err, 
		prefs.ConstructL();
		prefs.SetFileL(&file);
		RETURN_IF_ERROR(prefs.WriteIntL(identifier, UNI_L("Active"), enabled));
		prefs.CommitL(true,true);
	);
	return OpStatus::OK;
}

OP_STATUS TestGPU(unsigned int backend)
{
	if (g_vegaGlobals.vega3dDevice && g_vegaGlobals.vega3dDevice->getDeviceType() == backend)
		return OpStatus::OK;
	else
		return OpStatus::ERR;
}
