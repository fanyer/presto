/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: s; c-basic-offset: 2 -*-
**
** Copyright (C) 2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include <strings.h>

#include "adjunct/autoupdate/autoupdate_checker/adaptation_layer/system_utils.h"
#include "adjunct/autoupdate/autoupdate_checker/platforms/mac/includes/platform.h"

namespace opera_update_checker
{
  namespace system_utils
  {

    /* static */ void SystemUtils::Sleep(OAUCTime time)
    {
      usleep(time * 1000);
    }

    /* static */ int SystemUtils::strnicmp(const char* str1, const char* str2, size_t num_chars)
    {
      return strncasecmp(str1, str2, num_chars);
    }

  }
}
