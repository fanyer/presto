/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef GTK2_TOOLKIT_LOOP_H
#define GTK2_TOOLKIT_LOOP_H

#include "platforms/x11api/plugins/plugin_watchdog.h"
#include "platforms/x11api/plugins/toolkits/toolkit_loop.h"
#include "modules/util/adt/threadsafequeue.h"

typedef struct _GIOChannel GIOChannel;

class Gtk2ToolkitLoop : public ToolkitLoop
{
public:
	Gtk2ToolkitLoop();
	~Gtk2ToolkitLoop();

	virtual OP_STATUS Init(int read_fd, int write_fd);
	virtual OP_STATUS WatchReadDescriptor(DescriptorListener* listener);
	virtual OP_STATUS WatchWriteDescriptor(DescriptorListener* listener);
	virtual OP_STATUS Run();
	virtual void Exit();
	virtual OP_STATUS SendMessageOnMainThread(OpTypedMessage* message);

	OP_STATUS OnReadReady(int fd, bool& keep_listening);
	OP_STATUS OnWriteReady(int fd, bool& keep_listening);

	OP_STATUS FlushQueue();

private:
	OP_STATUS SetChannel(int fd, unsigned flags);

	int m_read_fd;
	DescriptorListener* m_read_listener;
	int m_write_fd;
	DescriptorListener* m_write_listener;
	ThreadSafeQueue<OpTypedMessage*> m_queue;
	PluginWatchdog m_watchdog;
};

#endif // GTK2_TOOLKIT_LOOP_H
