/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: s; c-basic-offset: 2 -*-
**
** Copyright (C) 2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
**
*/

#ifndef OAUC_SYSTEM_UTILS_H
# define OAUC_SYSTEM_UTILS_H

#include "adjunct/autoupdate/autoupdate_checker/common/common.h"

namespace opera_update_checker
{
  namespace system_utils
  {
    /** The class providing the functionality using the underlying system's infrastructure. */
    class SystemUtils
    {
    public:
      /** Should sleep the calling process for the given amount of time in miliseconds. */
      static void Sleep(OAUCTime time);
      /** Case insensitive strncmp(). */
      static int strnicmp(const char* str1, const char* str2, size_t num_chars);
    };
  }
}

#endif // OAUC_SYSTEM_UTILS_H
