// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2012 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Julien Picalausa
//

#include "core/pch.h"

//See proc_loader.h for documentation
#include "platforms/windows_common/utils/proc_loader.h"

//Please, keep this list ordered:
//The libraries appear in alphabetical order and procedures loaded from them appear under the corresponding library, also sorted alphabetically

READY_LIBRARY(dwmapi, UNI_L("dwmapi.dll"));
LIBRARY_CALL(dwmapi, HRESULT, WINAPI, DwmIsCompositionEnabled, (BOOL *pfEnabled), (pfEnabled), E_NOTIMPL);