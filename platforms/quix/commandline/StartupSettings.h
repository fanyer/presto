/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */

#ifndef QUIX_STARTUPSETTINGS_H
#define QUIX_STARTUPSETTINGS_H

class PosixLogListener;

/** 
 * Placeholder for global variables we want to keep separate
 * from the command line manager
 */
struct StartupSettings
{
public:
	StartupSettings()		
		:no_argb(FALSE)
		,debug_font(FALSE)
		,debug_font_filter(FALSE)
		,debug_keyboard(FALSE)
		,debug_locale(FALSE)
		,debug_mouse(FALSE)
		,debug_plugin(FALSE)
		,debug_atom(FALSE)
		,debug_x11_error(FALSE)
		,debug_shape(FALSE)
		,debug_lirc(FALSE)
		,debug_ime(FALSE)
		,debug_iwscan(FALSE)
		,debug_libraries(FALSE)
		,debug_camera(FALSE)
		,sync_event(FALSE)
		,crashlog_mode(FALSE)
		,watir_test(FALSE)
		,do_grab(FALSE)
		,shape_alpha_level(1)
		,posix_dns_logger(0)
		,posix_socket_logger(0)
		,posix_file_logger(0)
		,posix_async_logger(0)
	{}

public:
	BOOL no_argb;
	BOOL debug_clipboard;
	BOOL debug_font;
	BOOL debug_font_filter;
	BOOL debug_keyboard;
	BOOL debug_locale;
	BOOL debug_mouse;
	BOOL debug_plugin;
	BOOL debug_atom;
	BOOL debug_x11_error;
	BOOL debug_shape;
	BOOL debug_lirc;
	BOOL debug_ime;
	BOOL debug_iwscan;
	BOOL debug_libraries;
	BOOL debug_camera;
	BOOL sync_event;
	BOOL crashlog_mode;
	BOOL watir_test;
	BOOL do_grab;
	UINT32 shape_alpha_level;
	PosixLogListener* posix_dns_logger;
	PosixLogListener* posix_socket_logger;
	PosixLogListener* posix_file_logger;
	PosixLogListener* posix_async_logger;
	OpString8 our_binary;		// Captured from argv[0]
	OpString8 share_dir;		// Set by -sd or OPERA_DIR
	OpString8 binary_dir;		// Set by -bd or OPERA_BINARYDIR
	OpString8 personal_dir;		// Set by -pd or OPERA_PERSONALDIR
	OpString8 temp_dir;			// Set by TMPDIR
	OpString8 display_name;
	OpString8 session_id;
	OpString8 session_key;
};

extern StartupSettings g_startup_settings;

#endif // QUIX_STARTUPSETTINGS_H
