/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#include "core/pch.h"

// This is ugly but it's for sharing the same implementation with the checker.
#define BOOL NSBOOL
#include "adjunct/autoupdate/autoupdate_checker/platforms/mac/impl/globalstorageimpl.mm"
#undef BOOL
