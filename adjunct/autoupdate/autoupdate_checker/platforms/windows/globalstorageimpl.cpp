/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: s; c-basic-offset: 2 -*-
**
** Copyright (C) 2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "adjunct/autoupdate/autoupdate_checker/platforms/windows/includes/globalstorageimpl.h"
#include "adjunct/autoupdate/autoupdate_checker/adaptation_layer/global_storage.h"

namespace opera_update_checker { namespace global_storage {

/* static */ GlobalStorage* GlobalStorage::Create()
{
  OAUCGlobalStorageImpl* storage = OAUC_NEW(OAUCGlobalStorageImpl, ());
  if (storage)
  {
    if (storage->Initialize() != opera_update_checker::status::StatusCode::OK)
    {
      OAUC_DELETE(storage);
      return NULLPTR;
    }
  }

  return storage;
}

/* static */ void GlobalStorage::Destroy(GlobalStorage* storage)
{
  OAUC_DELETE(storage);
}

} }

using namespace opera_update_checker::status;

namespace
{
  const char* SUBKEY = "SOFTWARE\\Opera Software";
  const WCHAR* REGISTRY_ACCESS_MUTEX_NAME = L"oauc_registry_mutex";
}

OAUCGlobalStorageImpl::OAUCGlobalStorageImpl() : accessMutex_(INVALID_HANDLE_VALUE)
{
}

OAUCGlobalStorageImpl::~OAUCGlobalStorageImpl()
{
  if (accessMutex_ != INVALID_HANDLE_VALUE)
    CloseHandle(accessMutex_);
}

opera_update_checker::status::Status OAUCGlobalStorageImpl::Initialize()
{
  accessMutex_ = CreateMutex(NULL, FALSE, REGISTRY_ACCESS_MUTEX_NAME);
  if (accessMutex_ == NULL && GetLastError() == ERROR_ACCESS_DENIED)
    accessMutex_ = OpenMutex(SYNCHRONIZE, FALSE,REGISTRY_ACCESS_MUTEX_NAME);

  return accessMutex_ != NULL && accessMutex_ != INVALID_HANDLE_VALUE ? opera_update_checker::status::StatusCode::OK : opera_update_checker::status::StatusCode::FAILED;
}

namespace
{
  class MutexReleaser
  {
  private:
    HANDLE& mutex_;
  public:
    MutexReleaser(HANDLE& mutex) : mutex_(mutex) {
    }
    ~MutexReleaser() {
      ReleaseMutex(mutex_);
    }
  };
}

/* virtual */ opera_update_checker::status::Status OAUCGlobalStorageImpl::SetData(const char *key, const char* data, unsigned long data_len)
{
  if (!key)
    return StatusCode::INVALID_PARAM;

  HKEY reg_key;
  reg_key = HKEY_CURRENT_USER;

  HKEY handle;
  if (WaitForSingleObject(accessMutex_, RETURN_IMMEDIATELY) == WAIT_TIMEOUT)
    return StatusCode::FAILED;

  MutexReleaser mutexReleaser(accessMutex_);
  if (data)
  {
    if(RegCreateKeyExA(reg_key, SUBKEY, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &handle, NULL) == ERROR_SUCCESS)
    {
      Status status = StatusCode::OK;
      if (RegSetValueExA(handle, key, 0, REG_SZ, reinterpret_cast<const BYTE*>(data), strlen(data)) != ERROR_SUCCESS)
        status = StatusCode::FAILED;

      RegCloseKey(handle);
      return status;
    }
  }
  else
  {
    if(RegOpenKeyExA(reg_key, SUBKEY, 0, KEY_WRITE, &handle) == ERROR_SUCCESS)
    {
      RegDeleteValueA(handle, key);
      RegCloseKey(handle);
      return StatusCode::OK;
    }
  }

  return StatusCode::FAILED;
}

/* virtual */ opera_update_checker::status::Status OAUCGlobalStorageImpl::GetData(const char *key, const char*& data, unsigned long& data_len)
{
  data = NULLPTR;
  data_len = 0;
  if (!key)
    return StatusCode::INVALID_PARAM;

  HKEY reg_key;
  reg_key = HKEY_CURRENT_USER;

  char* data_buffer = NULLPTR;

  if (WaitForSingleObject(accessMutex_, RETURN_IMMEDIATELY) == WAIT_TIMEOUT)
    return StatusCode::FAILED;

  MutexReleaser mutexReleaser(accessMutex_);

  HKEY handle;
  Status status = StatusCode::OK;
  if(RegOpenKeyExA(reg_key, SUBKEY, 0, KEY_QUERY_VALUE, &handle) == ERROR_SUCCESS)
  {
    if (RegQueryValueExA(handle, key, NULLPTR, NULLPTR, NULLPTR, &data_len) == ERROR_SUCCESS)
    {
      data_buffer = OAUC_NEWA(char, data_len);
      if (!data_buffer)
      {
        data_len = 0;
        status = StatusCode::OOM;
      }
      else if (RegQueryValueExA(handle, key, NULLPTR, NULLPTR, reinterpret_cast<BYTE*>(data_buffer), &data_len) != ERROR_SUCCESS)
      {
        OAUC_DELETEA(data_buffer);
        data_buffer = NULLPTR;
        data_len = 0;
        status = StatusCode::FAILED;
      }
    }
    RegCloseKey(handle);
  }

  data = data_buffer;
  return status;
}