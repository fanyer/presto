/* -*- Mode: c; tab-width: 4; c-basic-offset: 4; -*-
 *
 * Copyright (C) 2007-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef POSIX_SELECT_CALLBACK_H
#define POSIX_SELECT_CALLBACK_H
/** @file posix_select_callback.h Socket-management call-backs for plugix.
 *
 * These call-backs are provided for the sake of ../plugix/, so they are only
 * relevant when it is in use.  Their declarations (and specification) should be
 * moved to plugix.  Each is called exactly once by one method of pluginWrapper,
 * in ../plugix/ns4plugin_interface/plugin_proxy/pluginwrapper.cpp (q.v.).
 */

#  ifdef POSIX_OK_SELECT_CALLBACK // API_POSIX_SELECT_CALLBACK
/** Add a callback for a pluginWrapper.
 *
 * If an earlier call to addSocketCallback() has been made with the same fd, and
 * not fully cancelled by calling removeSocketCallback for that fd and all bits
 * of flags, then cb and data are ignored (aside from an assertion that they're
 * NULL or match what was passed before); the flags for the existing callback
 * are updated instead.  (This is a backward compatibility choice to match what
 * unix-net and the older unix code did; it can be changed on request - since
 * PosixSelector supports having several listeners on one file - but you'll need
 * to decide sane modified semantics for removeSocketCallback !)
 *
 * @param fd File descriptor to be watched.
 * @param flags Indicates what to watch for; | together 1 for read, 2 for write.
 * @param cb The callback; receives fd, flags and data as parameters.
 * @param data Arbitrary datum you want passed to the callback.
 * @return True if callback added successfully, else false.
 */
bool addSocketCallback(int fd, int flags, void (*cb)(int fd, int flags, void *data), void * data);
/** Remove, or limit, a callback.
 *
 * The given file descriptor should be one for which a listener was previously
 * added by addSocketCallback (q.v.).
 *
 * @param fd File descriptor being watched.
 * @param flags Indicates what to stop watching for; 0 means everything, else |
 * together 1 for read, 2 for write.
 * @return False if some flags remain to be watched, but PosixSelector failed to
 * Watch() the remainder.
 */
bool removeSocketCallback(int fd, int flags);
#  endif // POSIX_OK_SELECT_CALLBACK
#endif // POSIX_SELECT_CALLBACK_H
