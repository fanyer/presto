/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: s; c-basic-offset: 2 -*-
**
** Copyright (C) 2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "adjunct/autoupdate/autoupdate_checker/adaptation_layer/system_info.h"
#include <sys/sysctl.h>

namespace opera_update_checker
{
  namespace system_info
  {
    /* static */ const char* SystemInfo::GetPackageType()
    {
      return "APP";
    }

    /* virtual */ const char* SystemInfo::GetArch()
    {
#ifdef __x86_64
  return "x86_64";
#else
  return "i386";
#endif
    }
  }
}
