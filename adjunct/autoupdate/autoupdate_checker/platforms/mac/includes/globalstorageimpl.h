/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: s; c-basic-offset: 2 -*-
**
** Copyright (C) 2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef GLOBALSTORAGE_IMPL_H
#define GLOBALSTORAGE_IMPL_H

#include "../../../common/common.h"
#include "../../../adaptation_layer/global_storage.h"

#define OAUC_GLOBAL_STORAGE_FILE    "OperaAUC.plist"

#ifndef MAX_OAUC_PRODUCT_LEN
#define MAX_OAUC_PRODUCT_LEN (128)
#endif

class OAUCGlobalStorageImpl : public opera_update_checker::global_storage::GlobalStorage
{
private:
  char opera_product_type[MAX_OAUC_PRODUCT_LEN];
public:
  virtual opera_update_checker::status::Status SetData(const char *key, const char* data, unsigned long data_len);
  virtual opera_update_checker::status::Status GetData(const char *key, const char*& data, unsigned long& data_len);
  const char* GetPrefsFile();
  const char* GetPrefsFolder();
  virtual void SetOperaProductType(const char* product_type);
};

#endif // GLOBALSTORAGE_IMPL_H
