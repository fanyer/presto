/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: s; c-basic-offset: 2 -*-
**
** Copyright (C) 2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "../includes/globalstorageimpl.h"

namespace opera_update_checker
{
  namespace global_storage
    {
      /* static */ GlobalStorage* GlobalStorage::Create()
      {
        return OAUC_NEW(OAUCGlobalStorageImpl, ());
      }

      /* static */ void GlobalStorage::Destroy(GlobalStorage* storage)
      {
        OAUC_DELETE(storage);
      }
  }
}

