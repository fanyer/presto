// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2012 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifdef POSIX_AUTOUPDATECHECKER_IMPLEMENTATION
#include <string.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <fstream>
#include "adjunct/autoupdate/autoupdate_checker/adaptation_layer/system_info.h"
#include "adjunct/autoupdate/autoupdate_checker/adaptation_layer/system_utils.h"

namespace opera_update_checker {
namespace system_info {

#define PACKAGE_NAME_LENGTH 32
class StringHolder {
 public:
  StringHolder() {
    read_uname_ = (uname(&sys_info_) == 0);
    read_package_name_ = false;
    // Package name must be read from an ini file :-(
    std::ifstream package_id_ini("/usr/share/opera/package-id.ini");
    if (package_id_ini.is_open() && package_id_ini.good()) {
      const char key_searched[] = "Package Type=";
      char key_buffer[sizeof(key_searched) + PACKAGE_NAME_LENGTH];
      // Go over the file until you've found "Package Type="
      int bytes_read = 0;
      while (!package_id_ini.eof()
            && !package_id_ini.fail()
            && bytes_read == 0) {
        package_id_ini.getline(key_buffer, sizeof(key_buffer));
        bytes_read = sscanf(key_buffer, "Package Type=%s", package_name_);
      }
      if (bytes_read > 0)
        read_package_name_ = true;
    }
  }
  bool read_uname_;
  bool read_package_name_;
  char package_name_[PACKAGE_NAME_LENGTH];
  utsname sys_info_;
};

const StringHolder g_stringHolder;

/** Returns operating system's name e.g. "Windows". Never NULL. */
/*static*/
const char* SystemInfo::GetOsName() {
  if (g_stringHolder.read_uname_)
    return g_stringHolder.sys_info_.sysname;
  else
    return "Unknown OS";
}

/** Returns operating system's version e.g. "7" for Windows 7. Never NULL. */
/*static*/
const char* SystemInfo::GetOsVersion() {
  if (g_stringHolder.read_uname_)
    return g_stringHolder.sys_info_.release;
  else
    return "Unknown Version";
}

/** Returns computer's architecture e.g. "x86". Never NULL. */
/*static*/
const char* SystemInfo::GetArch() {
  if (g_stringHolder.read_uname_)
    return g_stringHolder.sys_info_.machine;
  else
    return "Unknown Architecture";
}

/** Returns expected package type e.g. "EXE". Never NULL. */
/*static*/
const char* SystemInfo::GetPackageType() {
  if (g_stringHolder.read_package_name_)
    return g_stringHolder.package_name_;
  else
    return "Unknown Package Type";
}

}  // namespace system_info

namespace system_utils {

/** Sleep the calling process for the given amount of time in miliseconds. */
/*static*/
void SystemUtils::Sleep(OAUCTime time) {
  usleep(time * 1000);  // converting miliseconds to microseconds
}

/** Case insensitive strncmp(). */
/*static*/
int SystemUtils::strnicmp(const char* str1, const char* str2, size_t length) {
  return strncasecmp(str1, str2, length);
}

}  // namespace system_utils
}  // namespace opera_update_checker
#endif  // POSIX_AUTOUPDATECHECKER_IMPLEMENTATION
