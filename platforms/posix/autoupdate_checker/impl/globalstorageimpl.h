// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2012 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef PLATFORMS_POSIX_AUTOUPDATE_CHECKER_IMPL_GLOBALSTORAGEIMPL_H_
#define PLATFORMS_POSIX_AUTOUPDATE_CHECKER_IMPL_GLOBALSTORAGEIMPL_H_
#ifdef POSIX_AUTOUPDATECHECKER_IMPLEMENTATION

#include <sqlite3.h>
#include "adjunct/autoupdate/autoupdate_checker/adaptation_layer/global_storage.h"

namespace opera_update_checker {
namespace global_storage {

/** This implementation uses the lightweight sqlite3 database to store data.
 */
class GlobalStorageImpl : public GlobalStorage {
 public:
  GlobalStorageImpl();
  ~GlobalStorageImpl();
  bool Init();
  virtual status::Status SetData(const char *key,
                                 const char* data,
                                 unsigned long data_len);
  virtual status::Status GetData(const char *key,
                                 const char*& data,
                                 unsigned long& data_len);
 private:
  status::Status InsertOrUpdate(const char *key,
                                const char* data,
                                unsigned long data_len);
  status::Status Remove(const char *key);
  sqlite3* database_;
};
}  // namespace ipc
}  // namespace opera_update_checker

#endif  // POSIX_AUTOUPDATECHECKER_IMPLEMENTATION
#endif  // PLATFORMS_POSIX_AUTOUPDATE_CHECKER_IMPL_GLOBALSTORAGEIMPL_H_
