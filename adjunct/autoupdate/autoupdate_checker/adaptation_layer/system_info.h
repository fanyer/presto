/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: s; c-basic-offset: 2 -*-
**
** Copyright (C) 2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
**
*/

#ifndef OAUC_SYSTEM_INFO_H
# define OAUC_SYSTEM_INFO_H

namespace opera_update_checker
{
  namespace system_info
  {
    /** The class providing information about the system. */
    class SystemInfo
    {
    public:
      /** Returns operating system's name e.g. "Windows". Never NULL. */
      static const char* GetOsName();
      /** Returns operating system's version e.g. "7" for Windows 7. Never NULL. */
      static const char* GetOsVersion();
      /** Returns computer's architecture e.g. "x86". Never NULL. */
      static const char* GetArch();
      /** Returns expected package type e.g. "EXE". Never NULL. */
      static const char* GetPackageType();
    };
  }
}

#endif // OAUC_SYSTEM_INFO_H
