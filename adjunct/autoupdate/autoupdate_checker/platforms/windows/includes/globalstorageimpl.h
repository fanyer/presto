/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: s; c-basic-offset: 2 -*-
**
** Copyright (C) 2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef GLOBALSTORAGE_IMPL_H
# define GLOBALSTORAGE_IMPL_H

# include "adjunct/autoupdate/autoupdate_checker/common/common.h"
# include "adjunct/autoupdate/autoupdate_checker/adaptation_layer/global_storage.h"

class OAUCGlobalStorageImpl : public opera_update_checker::global_storage::GlobalStorage
{
  HANDLE accessMutex_;
public:
  opera_update_checker::status::Status Initialize();
  OAUCGlobalStorageImpl();
  ~OAUCGlobalStorageImpl();
  virtual opera_update_checker::status::Status SetData(const char *key, const char* data, unsigned long data_len);
  virtual opera_update_checker::status::Status GetData(const char *key, const char*& data, unsigned long& data_len);
};

#endif // GLOBALSTORAGE_IMPL_H