/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */
#include "core/pch.h"

#include "platforms/quix/commandline/StartupSettings.h"
#include "platforms/unix/base/common/lirc.h"
#include "modules/inputmanager/inputmanager.h"
#include "adjunct/desktop_util/string/stringutils.h"

#include <dlfcn.h>

#ifndef RTLD_DEFAULT
# warning "RTLD_DEFAULT not defined - defining it as 0"
# define RTLD_DEFAULT 0
#endif

Lirc* Lirc::m_lirc;


// BEGIN code taken from <lirc/lirc_client.h>

#define LIRC_ALL ((char *) (-1))

enum lirc_flags {
	none		= 0x00,
	once		= 0x01,
	quit		= 0x02,
	mode		= 0x04,
	ecno		= 0x08,
	startup_mode= 0x10
};

struct lirc_list
{
	char *string;
	struct lirc_list *next;
};

struct lirc_code
{
	char *remote;
	char *button;
	struct lirc_code *next;
};

struct lirc_config
{
	char *current_mode;
	struct lirc_config_entry *next;
	struct lirc_config_entry *first;
};

struct lirc_config_entry
{
	char *prog;
	struct lirc_code *code;
	unsigned int rep_delay;
	unsigned int rep;
	struct lirc_list *config;
	char *change_mode;
	unsigned int flags;

	char *mode;
	struct lirc_list *next_config;
	struct lirc_code *next_code;

	struct lirc_config_entry *next;
};

// END code taken from <lirc/lirc_client.h>


// static
OP_STATUS Lirc::Create()
{
	OP_STATUS rc = OpStatus::OK;
	if( !m_lirc )
	{
		m_lirc = OP_NEW(Lirc, ());
		if (!m_lirc)
			return OpStatus::ERR_NO_MEMORY;

		OP_STATUS rc = m_lirc->Start();
		if (OpStatus::IsError(rc) )
		{
			OP_DELETE(m_lirc);
		    m_lirc = 0;
		}
	}
	return rc;
}


// static
void Lirc::Destroy()
{
	OP_DELETE(m_lirc);
	m_lirc = 0;
}


Lirc::Lirc()
	: m_config(0)
	, m_lib_handle(0)
	, m_timer(0)
{
}

Lirc::~Lirc()
{
	if (lirc_freeconfig)
		lirc_freeconfig(m_config);

	if (lirc_deinit)
		lirc_deinit();

	if (m_lib_handle)
		dlclose(m_lib_handle);

	OP_DELETE(m_timer);
}

OP_STATUS Lirc::Start()
{
	bool syms_resolved = false;

	/* First try to resolve the LIRC symbols without opening the lirc_client
	   library (i.e. try to resolve them from the Opera executable). If that
	   fails, try to resolve them by looking them up in the lirc_client.so
	   library. If that fails, give up. */

	for (int i=0; i<2; i++)
	{
		if (i == 0)
			m_lib_handle = RTLD_DEFAULT;
		else
		{
			m_lib_handle = dlopen("liblirc_client.so.0", RTLD_NOW);
			if (!m_lib_handle)
				return OpStatus::ERR;
		}

		LOAD_SYMBOL(dlsym, m_lib_handle, lirc_init);
		LOAD_SYMBOL(dlsym, m_lib_handle, lirc_deinit);
		LOAD_SYMBOL(dlsym, m_lib_handle, lirc_readconfig);
		LOAD_SYMBOL(dlsym, m_lib_handle, lirc_freeconfig);
		LOAD_SYMBOL(dlsym, m_lib_handle, lirc_nextcode);
		LOAD_SYMBOL(dlsym, m_lib_handle, lirc_code2char);

		if (lirc_init && lirc_deinit && lirc_readconfig &&
			lirc_freeconfig && lirc_nextcode && lirc_code2char)
		{
			syms_resolved = true;
			break;
		}
	}

	if (!syms_resolved)
		return OpStatus::ERR;

	if (m_lib_handle == RTLD_DEFAULT)
		m_lib_handle = 0;

	int lircfd = lirc_init("opera", 0);
	if (lircfd == -1)
		return OpStatus::ERR;

	if (lirc_readconfig(0, &m_config, 0) != 0)
		return OpStatus::ERR;

	fcntl(lircfd, F_SETFL, fcntl(lircfd, F_GETFL, 0) | O_NONBLOCK);

	return g_posix_selector->Watch(lircfd, PosixSelector::READ,
# ifndef POSIX_CAP_SELECTOR_POLL // deprecates interval and removes timescale tweaks
								   POSIX_DUMMY_TIMESCALE,
# endif // POSIX_CAP_SELECTOR_POLL
								   this);
	// Base-class destructor takes care of un-Watch()ing.
}


void Lirc::OnTimeOut(OpTimer* timer)
{
	OP_STATUS err = Start();
	if( OpStatus::IsError(err) )
		m_timer->Start(10000);
}


// static
void Lirc::PrintEntry(const char* button, const char* action )
{
	printf(
		"begin\n"
		"\tprog = opera\n"
		"\tbutton = %s\n"
		"\tconfig = %s\n"
		"end\n"
		"\n", button, action );
}

// static
void Lirc::DumpLircrc()
{
	puts("# BEGIN lircrc entry for Opera\n"
		 "#\n"
		 // TRANSLATE_ME ! (just these first few lines, naturally)
		 "# This is an example configuration of a LIRC section for Opera.\n"
		 "# It needs to be adjusted to match your remote control button names, and\n"
		 "# otherwise configured for your needs. Save it in ~/.lircrc\n"
		 "# The values to use with 'config' keys are the same as the allowed values\n"
		 "# used in the Opera input action system (standard_keyboard.ini, etc.).\n"
		 "\n");
	PrintEntry("DOWN", "Navigate down");
	PrintEntry("UP", "Navigate up");
	PrintEntry("LEFT", "Navigate left");
	PrintEntry("RIGHT", "Navigate right");

	PrintEntry("OK", "Activate element");
	PrintEntry("PLAY", "Activate element | Open link");
	PrintEntry("<<", "Go to start | Back");
	PrintEntry(">>", "Go to end | Forward");

	PrintEntry("|<<", "Page up | Back");
	PrintEntry(">>|", "Page down | Forward | Fast forward");
	PrintEntry("6", "Zoom to, 100");
	PrintEntry("7", "Zoom out, 100");

	PrintEntry("8", "Zoom in, 100");
	PrintEntry("9", "Zoom out, 10");
	PrintEntry("0", "Zoom in, 10");
	PrintEntry("VOL_UP", "Zoom in, 10");
	PrintEntry("VOL_DOWN", "Zoom out, 10");

	PrintEntry("MENU", "Enter fullscreen | Leave fullscreen");
	PrintEntry("POWER", "Exit");

	puts("# END lircrc entry for Opera\n");
}

inline OP_STATUS Lirc::OnReadReady()
{
	char *code;
	if (lirc_nextcode(&code) == 0 && code)
	{
		if (g_startup_settings.debug_lirc)
			printf("opera [lirc]: Received code: %s\n", code);

		char *action;
		while (lirc_code2char(m_config, code, &action) == 0 && action)
		{
			if (g_startup_settings.debug_lirc)
				printf("opera [lirc]: Action: %s\n", action);

			OpString action_string;
			action_string.Set(action);

			OpAutoVector<OpString> list;
			StringUtils::SplitString( list, action_string, '|', FALSE );
			for( UINT32 i=0; i<list.GetCount(); i++ )
			{
				OpString8 action_opstring;
				action_opstring.Set(list.Get(i)->CStr());
				action_opstring.Strip();

				OpInputAction::Action action;
				INTPTR action_data = 0;
				int pos = action_opstring.Find(",");
				if( pos != KNotFound )
				{
					OpString8 argument;
					argument.Set(action_opstring.SubString(pos+1));
					action_opstring.Set(action_opstring.SubString(0, pos));
					if( argument.HasContent() )
					{
						int val;
						if( sscanf(argument.CStr(), "%d", &val) == 1 )
							action_data = val;
					}
				}

				if (OpStatus::IsSuccess(g_input_manager->GetActionFromString(action_opstring.CStr(), &action)))
				{
					BOOL ok = g_input_manager->InvokeAction(action, action_data);
					if( ok )
						break;
				}
			}
		}
		free(code);
	}
	else
	{
		if (g_startup_settings.debug_lirc)
			printf("opera [lirc]: Communication with lircd failed.  New attempt every 10 seconds.\n");

		g_posix_selector->Detach(this);

		if (lirc_deinit)
			lirc_deinit();

		if (lirc_freeconfig)
			lirc_freeconfig(m_config);
		m_config = 0;

		if (m_lib_handle)
			dlclose(m_lib_handle);
		m_lib_handle = 0;


		// Save data when timeout expires
		if( !m_timer )
		{
			m_timer = OP_NEW(OpTimer, ());
		}
		if( m_timer )
		{
			m_timer->SetTimerListener( this );
			m_timer->Start(10000);
		}

		return OpStatus::ERR;
	}

	return OpStatus::OK;
}

# ifdef POSIX_CAP_SELECTOR_FEEDBACK
bool
# else
void
# endif // POSIX_CAP_SELECTOR_FEEDBACK
Lirc::OnReadReady(int /* ignored */)
{
#if defined(DEBUG_ENABLE_OPASSERT) || defined(POSIX_CAP_SELECTOR_FEEDBACK)
	OP_STATUS res =
#endif
	OnReadReady();
	OP_ASSERT(OpStatus::IsSuccess(res));
#ifdef POSIX_CAP_SELECTOR_FEEDBACK
	return OpStatus::IsSuccess(res);
#endif
}

void Lirc::OnDetach(int fd)
{
	// anything worth doing ?  We'll be getting no further calls to OnReadReady().
}

void Lirc::OnError(int fd, int err /* =0 */)
{
	// anything worth doing ?
}

