/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Luca Venturi
**
*/

#include "core/pch.h"

#include "modules/url/url_man.h"
#include "modules/cache/cache_ctxman_multimedia.h"

// For now, Multimedia support requires a disk
#ifdef DISK_CACHE_SUPPORT

OP_STATUS Context_Manager_Multimedia::CreateManager(URL_CONTEXT_ID a_id, OpFileFolder a_vlink_loc, OpFileFolder a_cache_loc, BOOL share_cookies_with_main_context, int a_cache_size_pref)
{
    Context_Manager_Multimedia * OP_MEMORY_VAR mng = OP_NEW(Context_Manager_Multimedia, (a_id, a_vlink_loc, a_cache_loc));

	if(!mng)
		return OpStatus::ERR_NO_MEMORY;

	TRAPD(op_err, mng->ConstructPrefL(a_cache_size_pref, FALSE)); // Don't load the index, to avoid problems with coolies

	if(OpStatus::IsSuccess(op_err))
	{
		/// Create the context. Enforce vlink_loc to be the same as cookies_loc, as required by URL_Manager
		TRAPD(op_err, urlManager->AddContextL(a_id, mng, a_vlink_loc, share_cookies_with_main_context, TRUE));

		if(OpStatus::IsSuccess(op_err))
			return OpStatus::OK;
		else
		{
			OP_DELETE(mng);

			return op_err;
		}
	}
	else
	{
		OP_DELETE(mng);

		return op_err;
	}
}
#endif // DISK_CACHE_SUPPORT
