/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: s; c-basic-offset: 2 -*-
**
** Copyright (C) 2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "adaptation_layer/system_info.h"
#include "platforms/windows/includes/platform.h"

namespace opera_update_checker { namespace system_info {

/* static */const char* SystemInfo::GetOsName()
{
  return "Windows";
}

/* static */ const char* SystemInfo::GetOsVersion()
{
  OSVERSIONINFO osvi;
  ZeroMemory(&osvi, sizeof(osvi));
  osvi.dwOSVersionInfoSize = sizeof(osvi);

  // Get OS version.
  if (!GetVersionEx(&osvi))
  {
    return NULL;
  }
  else if ((osvi.dwMajorVersion >= 6) && (osvi.dwMinorVersion >= 2))
  {
    return "8";
  }
  else if ((osvi.dwMajorVersion >= 6) && (osvi.dwMinorVersion >= 1))
  {
    return "7";
  }
  else if (osvi.dwMajorVersion >= 6) // Vista or newer, presumably compatible.
  {
    return "Vista";
  }
  else if ( (osvi.dwMajorVersion == 5) && (osvi.dwMinorVersion >= 1) ) // Server is 5.2, but XP compatible.
  {
    return "XP";
  }
  else if (osvi.dwMajorVersion == 5)
  {
    return "2000";
  }

  return "9x";

}

/* static */ const char* SystemInfo::GetArch()
{
#ifdef _WIN64
  return "x64";
#else // _WIN64
  return "i386";
#endif // _WIN64
}

/* static */ const char* SystemInfo::GetPackageType()
{
  return "EXE";
}

} }