/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4; -*-
 *
 * Copyright (C) 2007-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"
#ifdef POSIX_OK_SELECT_CALLBACK
#include "platforms/posix/posix_select_callback.h"
#include "platforms/posix/posix_selector.h"

class PluginCallbackListener : public PosixSelectListener, public Link
{
	void (*m_cb)(int fd, int flags, void* data);
	void * m_data;
	int m_flag;
	int m_fd;
public:
	PluginCallbackListener(int fd, int flag, void (*cb)(int fd, int flags, void *data), void * data)
		: m_cb(cb), m_data(data), m_flag(flag & 3), m_fd(fd) {}

	bool Carries(int fd) { return m_fd == fd; }
	bool Augment(int fd, int flags, void (*cb)(int fd, int flags, void *data), void * data)
	{
		if (fd != m_fd || (cb != 0 && cb != m_cb) || (data != 0 && data != m_data))
			OP_ASSERT(!"Augmenting socket-callback with mismatching data");

		flags &= 3;
		if (flags & ~m_flag) // else nothing to do
		{
			m_flag |= flags;
			g_posix_selector->SetMode(this, m_fd, Mode());
		}
		return true;
	}
	bool Clear(int flags)
	{
		m_flag &= ~flags;
		if (m_flag)
			g_posix_selector->SetMode(this, m_fd, Mode());
		else
		{
			Out();
			OP_DELETE(this); // Detach()es
		}
		return true;
	}
	PosixSelector::Type Mode()
	{
		unsigned int mode = 0;
		if (m_flag & 1) mode |= PosixSelector::READ;
		if (m_flag & 2) mode |= PosixSelector::WRITE;
		return (PosixSelector::Type)mode;
	}

	virtual void OnError(int fd, int err=0) {}
	virtual void OnDetach(int fd) { Clear(m_flag); }

	virtual void OnReadReady(int fd)
	{
		OP_ASSERT(fd == m_fd && (m_flag & 1));
		(*m_cb)(fd, 1, m_data);
	}
	virtual void OnWriteReady(int fd)
	{
		OP_ASSERT(fd == m_fd && (m_flag & 2));
		(*m_cb)(fd, 2, m_data);
	}
};

// static
PluginCallbackListener * PosixModule::FindPluginCB(int fd)
{
	if (g_opera)
		for (PluginCallbackListener *run = static_cast<PluginCallbackListener *>(g_opera->posix_module.m_sock_cbs.First());
			 run; run = static_cast<PluginCallbackListener *>(run->Suc()))
			if (run->Carries(fd))
				return run;
	return 0;
}
// static
bool PosixModule::SocketWatch(int fd, class PluginCallbackListener *what)
{
	if (OpStatus::IsError(g_posix_selector->Watch(fd, what->Mode(), what)))
		return false;

	OP_ASSERT(g_opera); // else Watch should have failed.
	what->Into(&(g_opera->posix_module.m_sock_cbs));
	return true;
}

bool addSocketCallback(int fd, int flags, void (*cb)(int fd, int flags, void *data), void * data)
{
	OP_ASSERT(!(flags & ~3) && fd != -1);
	if (PluginCallbackListener *old = PosixModule::FindPluginCB(fd))
		return old->Augment(fd, flags, cb, data);

	PluginCallbackListener * fresh = OP_NEW(PluginCallbackListener, (fd, flags, cb, data));
	if (fresh)
		if (PosixModule::SocketWatch(fd, fresh))
			return true;
		else
			OP_DELETE(fresh);

	return false;
}
bool removeSocketCallback(int fd, int flags)
{
	OP_ASSERT(!(flags & ~3) && fd != -1);
	if (PluginCallbackListener *old = PosixModule::FindPluginCB(fd))
		return old->Clear(flags ? flags : 3); // may delete old

	return true; // nothing to do
}
#endif // POSIX_OK_SELECT_CALLBACK
