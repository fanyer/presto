/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: s; c-basic-offset: 2 -*-
**
** Copyright (C) 2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
**
*/

#ifndef OAUC_COMMON_INCLUDES_H
#define OAUC_COMMON_INCLUDES_H

#include OAUC_PLATFORM_INCLUDES /* It has to be defined in the environment */

namespace opera_update_checker
{
  namespace status
  {
    /** Status codes returned by the API. */
    typedef int Status;
    class StatusCode
    {
    public:
      enum
      {
        NOT_SUPPORTED = -255,
        INVALID_PARAM,
        OOM,
        FAILED,
        TIMEOUT,
        OK = 0,
        TRY_AGAIN_LATER
      };
    };
  }
}

#endif // OAUC_COMMON_INCLUDES_H
