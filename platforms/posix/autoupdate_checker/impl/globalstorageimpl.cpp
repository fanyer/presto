// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2012 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifdef POSIX_AUTOUPDATECHECKER_IMPLEMENTATION
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <cassert>
#include <cstring>
#include "platforms/posix/autoupdate_checker/impl/globalstorageimpl.h"

using opera_update_checker::global_storage::GlobalStorage;
using opera_update_checker::global_storage::GlobalStorageImpl;
using opera_update_checker::status::Status;
using opera_update_checker::status::StatusCode;

GlobalStorageImpl::GlobalStorageImpl() : database_(NULLPTR) {
}

GlobalStorageImpl::~GlobalStorageImpl() {
  sqlite3_close(database_);
}

bool GlobalStorageImpl::Init() {
  char* home_dir = getenv("HOME");
  if (!home_dir) {
    // We don't know there to put the sqlite file
    return false;
  }
  const size_t kMaxSqliteFileLen = 256;
  char sqlite_file[kMaxSqliteFileLen];

  // Create $HOME/.opera if there's none
  snprintf(sqlite_file,
           kMaxSqliteFileLen,
           "%s/.opera",
           home_dir);
  int status = mkdir(sqlite_file, 0700);
  if (status == -1 && errno != EEXIST) {
    // If mkdir failed because the folder exists already, it's fine. Otherwise:
    return false; // Could not create $HOME/.opera
  }

  snprintf(sqlite_file,
           kMaxSqliteFileLen,
           "%s/.opera/OperaAutoupdateChecker.sqlite",
           home_dir);
  status &= sqlite3_open(sqlite_file, &database_);
  sqlite3_stmt* statement;
  status &= sqlite3_prepare_v2(
        database_,
        "CREATE TABLE IF NOT EXISTS store"
        "(key STRING PRIMARY KEY, value STRING);"
        "CREATE UNIQUE INDEX idx ON store(key);",
        -1,
        &statement,
        0);
  status &= sqlite3_step(statement);
  status &= sqlite3_finalize(statement);
  return status == SQLITE_OK;
}

Status GlobalStorageImpl::SetData(const char *key,
                               const char* data,
                               unsigned long data_len) {
  if (data) {
    return InsertOrUpdate(key, data, data_len);
  } else {
    return Remove(key);
  }
}

Status GlobalStorageImpl::InsertOrUpdate(const char *key,
                              const char* data,
                              unsigned long data_len) {
  sqlite3_stmt* statement;
  int status = sqlite3_prepare_v2(
          database_,
          "INSERT OR REPLACE INTO store VALUES (?,?);",
          -1,
          &statement,
          0);
  status &= sqlite3_bind_text(statement, 1, key, -1, SQLITE_TRANSIENT);
  status &= sqlite3_bind_text(statement, 2, data, data_len, SQLITE_TRANSIENT);
  status &= sqlite3_step(statement);
  status &= sqlite3_finalize(statement);
  return status == SQLITE_OK ? StatusCode::OK : StatusCode::FAILED;
}

Status GlobalStorageImpl::Remove(const char *key) {
  sqlite3_stmt* statement;
  int status = sqlite3_prepare_v2(
          database_,
          "DELETE FROM store WHERE key=?;",
          -1,
          &statement,
          0);
  status &= sqlite3_bind_text(statement, 1, key, -1, SQLITE_TRANSIENT);
  status &= sqlite3_step(statement);
  status &= sqlite3_finalize(statement);
  return status == SQLITE_OK ? StatusCode::OK : StatusCode::FAILED;
}

Status GlobalStorageImpl::GetData(const char *key,
                                  const char*& data,
                                  unsigned long& data_len) {
  sqlite3_stmt* statement;
  int status = sqlite3_prepare_v2(
          database_,
          "SELECT value FROM store WHERE key = ?;",
          -1,
          &statement,
          0);
  status &= sqlite3_bind_text(statement, 1, key, -1, SQLITE_TRANSIENT);
  int result = sqlite3_step(statement);
  if (result == SQLITE_ROW && sqlite3_column_count(statement) > 0) {
    const char* value =
        reinterpret_cast<const char*>(sqlite3_column_text(statement, 0));
    /* Can't just return the pointer sqlite3 gave us, it'll be deleted
     * after the next call to sqlite3_step or sqlite3_finalize (ie. soon).
     * Need to allocate a new array and copy data. Caller takes ownership. */
    if (value) {
      data_len = strlen(value) + 1;  // +1 for trailing null char
      char* output = OAUC_NEWA(char, data_len);
      if (output) {
        memcpy(output, value, data_len);
        data = output;
      }
    }
  }
  status &= sqlite3_finalize(statement);
  if (!data)
    data_len = 0;
  return status == SQLITE_OK ? StatusCode::OK : StatusCode::FAILED;
}

/*static */
GlobalStorage* GlobalStorage::Create() {
  GlobalStorageImpl* obj = OAUC_NEW( GlobalStorageImpl, ());
  if (obj && obj->Init())
    return obj;
  OAUC_DELETE(obj);
  return NULLPTR;
}

/*static */
void GlobalStorage::Destroy(GlobalStorage* storage) {
  OAUC_DELETE(storage);
}

#endif  // POSIX_AUTOUPDATECHECKER_IMPLEMENTATION
