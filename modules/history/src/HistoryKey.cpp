/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 * psmaas - Patricia Aas
 */

#include "core/pch.h"

#ifdef HISTORY_SUPPORT

#include "modules/history/src/HistoryKey.h"

#ifdef HISTORY_DEBUG
INT HistoryKey::m_number_of_keys = 0;
OpVector<HistoryKey> HistoryKey::keys;
#endif //HISTORY_DEBUG

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
HistoryKey * HistoryKey::Create(const OpStringC& key)
{
	OpAutoPtr<HistoryKey> history_key(OP_NEW(HistoryKey, ()));

	if(!history_key.get())
		return 0;

	RETURN_VALUE_IF_ERROR(history_key->m_key.Set(key.CStr()), 0);

    return history_key.release();
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
HistoryKey::~HistoryKey()
{
#ifdef HISTORY_DEBUG
	keys.RemoveByItem(this);
	m_number_of_keys--;
#endif //HISTORY_DEBUG

    OP_ASSERT(!InUse());   
}


/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
HistoryKey::HistoryKey() 
{
#ifdef HISTORY_DEBUG
	keys.Add(this);
	m_number_of_keys++;
#endif //HISTORY_DEBUG
}

#endif // HISTORY_SUPPORT
