/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 * Authors: Petter Nilsen
 */

#ifndef DESKTOP_PERMISSION_CALLBACK_H
#define DESKTOP_PERMISSION_CALLBACK_H

#include "modules/windowcommander/OpWindowCommander.h"

#ifdef WIC_PERMISSION_LISTENER
// this wraps an instance of a PermissionCallback so it can be queued and handled one at a time
class DesktopPermissionCallback : public Link
{
public:
	DesktopPermissionCallback(OpPermissionListener::PermissionCallback *callback, OpString& hostname);
	DesktopPermissionCallback(OpPermissionListener::PermissionCallback *callback) : m_permission_callback(callback) {}
	
	OpPermissionListener::PermissionCallback * GetPermissionCallback() { return m_permission_callback; }
	OpStringC GetAccessingHostname() { return m_accessing_hostname; }
	OP_STATUS SetAccessingHostname(const OpStringC& hostname) { return m_accessing_hostname.Set(hostname.CStr()); }
	OP_STATUS AddWebHandlerRequest(const OpStringC& request);
	const OpVector<OpString>& GetWebHandlerRequest() const { return m_web_handler_request; }

private:
	OpPermissionListener::PermissionCallback*	m_permission_callback;		// the core permission callback
	OpString									m_accessing_hostname;		// the accessing (requesting) host name
	OpAutoVector<OpString>						m_web_handler_request;
};

class DesktopPermissionCallbackQueue : private Head
{
public:
	virtual ~DesktopPermissionCallbackQueue() { Head::Clear(); }

	DesktopPermissionCallback*	Last() const { return static_cast<DesktopPermissionCallback*>(Head::Last()); }
	DesktopPermissionCallback*	First() const { return static_cast<DesktopPermissionCallback*>(Head::First()); }

	UINT GetCount() { return Cardinal(); }
	BOOL IsEmpty() { return Head::Empty(); }
	void AddItem(DesktopPermissionCallback* item) { item->Into(this); }
	void RemoveItem(DesktopPermissionCallback* item);
	void DeleteItem(DesktopPermissionCallback* item);
	
	BOOL HasType(OpPermissionListener::PermissionCallback::PermissionType type);
};
#endif // WIC_PERMISSION_LISTENER
#endif // DESKTOP_PERMISSION_CALLBACK_H
