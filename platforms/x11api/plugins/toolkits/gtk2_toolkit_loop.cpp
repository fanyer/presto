/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#if defined(X11API) && defined(NS4P_COMPONENT_PLUGINS)

#include "platforms/x11api/plugins/toolkits/gtk2_toolkit_loop.h"

#include "platforms/utilix/OpGtk.h"

namespace Gtk2ToolkitFunctions
{
	static gboolean OnChannelActivity(GIOChannel* source, GIOCondition condition, gpointer data);
	static void ScheduleRunSlice(unsigned timeout);
	static gboolean RunSlice(gpointer data);
	static gboolean FlushQueue(gpointer data);
	static gboolean KickWatchdog(gpointer data);

	static guint runslice_timeout = 0;
	static int runslice_level = 0;
};

Gtk2ToolkitLoop::Gtk2ToolkitLoop()
	: m_read_fd(-1)
	, m_read_listener(NULL)
	, m_write_fd(-1)
	, m_write_listener(NULL)
{
}

Gtk2ToolkitLoop::~Gtk2ToolkitLoop()
{
	OpGtk::UnloadGtk();

	while (!m_queue.IsEmpty())
		OP_DELETE(m_queue.PopFirst());
}

OP_STATUS Gtk2ToolkitLoop::Init(int read_fd, int write_fd)
{
	m_read_fd = read_fd;
	m_write_fd = write_fd;

	RETURN_IF_ERROR(m_queue.Init());

	if (!OpGtk::LoadGtk(false))
	{
		OP_ASSERT(!"Can't load GTK");
		return OpStatus::ERR;
	}

	if (!OpGtk::gtk_init_check(0, NULL))
	{
		OP_ASSERT(!"gtk_init_check failed");
		return OpStatus::ERR;
	}

	return OpStatus::OK;
}

OP_STATUS Gtk2ToolkitLoop::WatchReadDescriptor(DescriptorListener* listener)
{
	if (m_read_listener)
		return OpStatus::OK;

	RETURN_IF_ERROR(SetChannel(m_read_fd, G_IO_IN | G_IO_PRI));
	m_read_listener = listener;
	return OpStatus::OK;
}

OP_STATUS Gtk2ToolkitLoop::WatchWriteDescriptor(DescriptorListener* listener)
{
	if (m_write_listener)
		return OpStatus::OK;

	RETURN_IF_ERROR(SetChannel(m_write_fd, G_IO_OUT));
	m_write_listener = listener;
	return OpStatus::OK;
}

OP_STATUS Gtk2ToolkitLoop::SetChannel(int fd, unsigned flags)
{
	GIOChannel* channel = OpGtk::g_io_channel_unix_new(fd);
	if (!channel)
		return OpStatus::ERR_NO_MEMORY;

	if (!OpGtk::g_io_add_watch(channel, (GIOCondition)(flags | G_IO_ERR | G_IO_HUP | G_IO_NVAL),
		(GIOFunc)&Gtk2ToolkitFunctions::OnChannelActivity, this))
		return OpStatus::ERR;

	// reference count was increased by g_io_add_watch, we no longer care about the channel
	OpGtk::g_io_channel_unref(channel);

	return OpStatus::OK;
}

gboolean Gtk2ToolkitFunctions::OnChannelActivity(GIOChannel* source, GIOCondition condition, gpointer data)
{
	Gtk2ToolkitLoop* loop = static_cast<Gtk2ToolkitLoop*>(data);
	int fd = OpGtk::g_io_channel_unix_get_fd(source);
	gboolean success = !(condition & (G_IO_ERR | G_IO_HUP | G_IO_NVAL));
	bool keep_listening = true;

	if (success && condition & (G_IO_IN | G_IO_PRI))
		success = success && OpStatus::IsSuccess(loop->OnReadReady(fd, keep_listening));

	if (success && condition & G_IO_OUT)
		success = success && OpStatus::IsSuccess(loop->OnWriteReady(fd, keep_listening));

	if (!success)
		OpGtk::gtk_main_quit();

	if (OpGtk::gtk_main_level() <= 1) // If we're in the main loop, do a runslice
		Gtk2ToolkitFunctions::ScheduleRunSlice(0);

	return success && keep_listening;
}

OP_STATUS Gtk2ToolkitLoop::OnReadReady(int fd, bool& keep_listening)
{
	OP_ASSERT(fd == m_read_fd);
	if (!m_read_listener)
	{
		keep_listening = false;
		return OpStatus::OK;
	}

	RETURN_IF_ERROR(m_read_listener->OnReadReady(keep_listening));
	if (!keep_listening)
		m_read_listener = NULL;

	return OpStatus::OK;
}

OP_STATUS Gtk2ToolkitLoop::OnWriteReady(int fd, bool& keep_listening)
{
	OP_ASSERT(fd == m_write_fd);
	if (!m_write_listener)
	{
		keep_listening = false;
		return OpStatus::OK;
	}

	RETURN_IF_ERROR(m_write_listener->OnWriteReady(keep_listening));
	if (!keep_listening)
		m_write_listener = NULL;

	return OpStatus::OK;
}

OP_STATUS Gtk2ToolkitLoop::Run()
{
	Gtk2ToolkitFunctions::RunSlice(0);

	if (!op_getenv("OPERA_KEEP_BLOCKED_PLUGIN"))
	{
		RETURN_IF_ERROR(m_watchdog.Init());
		RETURN_IF_ERROR(m_watchdog.Start(X11API_PLUGIN_WATCHDOG_TIMEOUT * 1000));
		OpGtk::g_timeout_add(1000, &Gtk2ToolkitFunctions::KickWatchdog, &m_watchdog);
	}

	OpGtk::gtk_main();

	return OpStatus::OK;
}

void Gtk2ToolkitFunctions::ScheduleRunSlice(unsigned timeout)
{
	if (runslice_timeout)
		OpGtk::g_source_remove(runslice_timeout);

	if (timeout == UINT_MAX)
		runslice_timeout = 0;
	else
		runslice_timeout = OpGtk::g_timeout_add(timeout, &Gtk2ToolkitFunctions::RunSlice, 0);
}

gboolean Gtk2ToolkitFunctions::RunSlice(gpointer data)
{
	runslice_level ++;

	// Do a runslice and schedule a new one
	unsigned waitms = g_component_manager->RunSlice();
	ScheduleRunSlice(waitms);

	runslice_level --;

	return FALSE;
}

gboolean Gtk2ToolkitFunctions::KickWatchdog(gpointer data)
{
	// Do not keep plugin alive if parent (opera) no longer exists
	// or if we are calling from a run slice. The latter is an indication
	// that the plugin is blocked. See DSK-375488

	if (getppid() == 1)
	{
		return TRUE;
	}
	else if (runslice_level >= 1)
	{
		return TRUE;
	}

	PluginWatchdog* watchdog = static_cast<PluginWatchdog*>(data);
	watchdog->Kick();

	return TRUE;
}

void Gtk2ToolkitLoop::Exit()
{
	OpGtk::gtk_main_quit();
}

OP_STATUS Gtk2ToolkitLoop::SendMessageOnMainThread(OpTypedMessage* message)
{
	RETURN_IF_ERROR(m_queue.Append(message));
	OpGtk::g_idle_remove_by_data(this);
	OpGtk::g_idle_add(&Gtk2ToolkitFunctions::FlushQueue, this);

	return OpStatus::OK;
}

gboolean Gtk2ToolkitFunctions::FlushQueue(gpointer data)
{
	OP_STATUS status = static_cast<Gtk2ToolkitLoop*>(data)->FlushQueue();

	// If there was an error we want to receive this event again
	return OpStatus::IsError(status);
}

OP_STATUS Gtk2ToolkitLoop::FlushQueue()
{
	if (m_queue.IsEmpty())
		return OpStatus::OK;

	do
	{
		OpTypedMessage* message = m_queue.PopFirst();
		RETURN_IF_ERROR(g_component_manager->SendMessage(message));
	} while (!m_queue.IsEmpty());

	Gtk2ToolkitFunctions::ScheduleRunSlice(0);

	return OpStatus::OK;
}

#endif // X11API && NS4P_COMPONENT_PLUGINS
