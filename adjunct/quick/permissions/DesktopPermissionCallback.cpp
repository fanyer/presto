/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 * Author: Petter Nilsen
 */
 
#include "core/pch.h"

#ifdef WIC_PERMISSION_LISTENER

#include "adjunct/quick/permissions/DesktopPermissionCallback.h" 


DesktopPermissionCallback::DesktopPermissionCallback(OpPermissionListener::PermissionCallback *callback, OpString& hostname):m_permission_callback(callback)
{
	m_accessing_hostname.Set(hostname);
}

OP_STATUS DesktopPermissionCallback::AddWebHandlerRequest(const OpStringC& request)
{
	OpString* s = OP_NEW(OpString,());
	if(!s || OpStatus::IsError(s->Set(request)) || OpStatus::IsError(m_web_handler_request.Add(s)))
	{
		OP_DELETE(s);
		return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}

void DesktopPermissionCallbackQueue::RemoveItem(DesktopPermissionCallback* item)
{ 
		OP_ASSERT(HasLink(item));

		if(HasLink(item))
		{
			item->Out();
		}
}

void DesktopPermissionCallbackQueue::DeleteItem(DesktopPermissionCallback* item) 
{ 
	RemoveItem(item);
	OP_DELETE(item);
}
	
BOOL DesktopPermissionCallbackQueue::HasType(OpPermissionListener::PermissionCallback::PermissionType type)
{
	DesktopPermissionCallback *item = First();
	while(item)
	{
		if(item->GetPermissionCallback() && item->GetPermissionCallback()->Type() == type)
		{
			return TRUE;
		}
		item = static_cast<DesktopPermissionCallback*>(item->Suc());
	}
	return FALSE;
}

#endif // WIC_PERMISSION_LISTENER
