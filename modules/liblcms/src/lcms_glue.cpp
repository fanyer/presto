/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef LCMS_3P_SUPPORT

#include "modules/liblcms/liblcms_constant.h"

#ifdef LCMS_CAM02_SUPPORT
#include "cmscam02.c"
#endif // LCMS_CAM02_SUPPORT

#ifdef LCMS_CGATS_SUPPORT
#include "cmscgats.c"
#endif // LCMS_CGATS_SUPPORT

#include "cmscnvrt.c"
#include "cmserr.c"
#include "cmsgamma.c"
#include "cmsgmt.c"
#include "cmsintrp.c"
#include "cmsio0.c"
#include "cmsio1.c"
#include "cmslut.c"

#ifdef LCMS_WRITE_SUPPORT
#include "cmsmd5.c"
#endif // LCMS_WRITE_SUPPORT

#include "cmsmtrx.c"
#include "cmsnamed.c"
#include "cmsopt.c"
#include "cmspack.c"
#include "cmspcs.c"
#include "cmsplugin.c"

#ifdef LCMS_PSLVL2_SUPPORT
#include "cmsps2.c"
#endif // LCMS_PSLVL2_SUPPORT

#include "cmssamp.c"

#ifdef LCMS_GBD_SUPPORT
#include "cmssm.c"
#endif // LCMS_GBD_SUPPORT

#include "cmstypes.c"
#include "cmsvirt.c"
#include "cmswtpnt.c"
#include "cmsxform.c"

#endif // LCMS_3P_SUPPORT
