/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef POSIX_CAPABILITIES_H
#define POSIX_CAPABILITIES_H __FILE__

/* Capabilities (newest first): */

/** Added PosixModule::InitLocale() and PosixNativeUtil::TransientLocale. */
#define POSIX_CAP_LOCALE_INIT

/** PosixLocale::{To,From}Native return OP_STATUS and change signature */
#define POSIX_CAP_NATIVE_STATUS

/** PosixModule provides OnCoreThread() instead of GetMainThread(). */
#define POSIX_CAP_ONCORE

/** PosixSelector provides Button, defines virtual Poll(), changes Watch() signature. */
#define POSIX_CAP_SELECTOR_POLL

/** PosixSelector has the SetMode() methods and its Type has a NONE mode. */
#define POSIX_CAP_SELECTOR_SETMODE

/** Reverted: PosixSelectListener's On*Ready briefly returned bool but are back to void. */
#undef POSIX_CAP_SELECTOR_FEEDBACK

/** Separated PosixSocketAddress's extensions out into PosixNetworkAddress */
#define POSIX_CAP_NETWORK_ADDRESS

/** PosixSystemInfo provides GetUserLanguages and GetUserCountry */
#define POSIX_CAP_USER_LANGUAGE

/** Support for sockets, socket addresses and host resolvers (2007/Aug). */
#define POSIX_CAP_SOCKET

/** PosixSystemInfo provides OpFileLengthToString */
#define POSIX_CAP_FILE_LENGTH_SYSIO

#endif /* POSIX_CAPABILITIES_H */
