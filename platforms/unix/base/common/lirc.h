/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2005-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */
#ifndef LIRC_H
#define LIRC_H

// May be able to turn this into a PosixSelectListener ...
#include "platforms/posix/posix_selector.h"
#include "platforms/utilix/dlmacros.h"
#include "modules/hardcore/timer/optimer.h"

class Lirc : public PosixSelectListener, public OpTimerListener
{
	OP_STATUS OnReadReady(); // Inlined before its eponymous caller, so both flavours share it.

public:

	static OP_STATUS Create();
	static void Destroy();
	static void DumpLircrc();

	virtual
# ifdef POSIX_CAP_SELECTOR_FEEDBACK
	bool
# else
	void
# endif // POSIX_CAP_SELECTOR_FEEDBACK
	OnReadReady(int fd);

	virtual void OnDetach(int fd);
	virtual void OnError(int fd, int err=0);

	/**
	 * Try to reconnect
	 */
	void OnTimeOut(OpTimer* timer);

private:
	Lirc();
	~Lirc();

	OP_STATUS Start();

	static void PrintEntry(const char* button, const char* action );

	DEFINE_SYMBOL(int, lirc_init, (const char *prog,int verbose));
	DEFINE_SYMBOL(int, lirc_deinit, (void));
	DEFINE_SYMBOL(int, lirc_readconfig, (char *file,struct lirc_config **config, int (check)(char *s)));
	DEFINE_SYMBOL(void, lirc_freeconfig, (struct lirc_config *config));
	DEFINE_SYMBOL(int, lirc_nextcode, (char **code));
	DEFINE_SYMBOL(int, lirc_code2char, (struct lirc_config *config,char *code, char **string));

private:
	struct lirc_config* m_config;
	void* m_lib_handle;
	OpTimer* m_timer;

private:
	static Lirc* m_lirc;
};

#endif // LIRC_H
