/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: s; c-basic-offset: 2 -*-
**
** Copyright (C) 2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
**
*/

#ifndef OAUC_GLOBAL_STORAGE_H
#define OAUC_GLOBAL_STORAGE_H

#include "adjunct/autoupdate/autoupdate_checker/common/common.h"

namespace opera_update_checker
{
  namespace global_storage
  {
    static const char* UUID_KEY = "UUID";
    static const char* LUT_KEY = "LUT";

    /** The class representing a system global storage e.g. the system registry on Windows. */
    class GlobalStorage
    {
    private:
      GlobalStorage(GlobalStorage&) {}
    protected:
      GlobalStorage() {}
      virtual ~GlobalStorage() {}
    public:
      /** The factory method. */
      static GlobalStorage* Create();
      /** The terminator */
      static void Destroy(GlobalStorage* storage);
      /** Sets Opera product name. Function is used only on OsX for meet
       *  Apple sandbox policy.
       *
       * @param[in] product_type - Opera product name.
       */
      virtual void SetOperaProductType(const char* product_type) {};
      /** Stores the data identified by the key in the storage.
       *
       * If the data, key pair entry already exists its data is updated.
       * Otherwise a new entry with the data, key pair is created and stored.
       *
       * @param[in] key - the key to identify the data by. Shouldn't be NULL.
       * @param[in] data - the data to be set. May be NULL in which case the given key,data pair entry should be deleted.
       * @param[in] data_len - the length of the data.
       *
       * @return StatusCode
       * @see StatusCode
       */
      virtual status::Status SetData(const char *key, const char* data, unsigned long data_len) = 0;
      /** Gets the data associated with the given key (NULL if there's no such data).
       * The caller is responsible for deleting the returned buffer.
       *
       * @param[in] key - the key to identify the data by. Shouldn't be NULL.
       * @param[in] data - a pointer to the buffer the data is put in.
       * @param[out] data_len - the length of the data.
       *
       * @return StatusCode
       * @see StatusCode
       */
      virtual status::Status GetData(const char *key, const char*& data, unsigned long& data_len) = 0;
    };
  }
}

#endif // OAUC_GLOBAL_STORAGE_H
