/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 */

#ifndef WANDSECURITYWRAPPER_H
#define WANDSECURITYWRAPPER_H

class Window;
class OpWindow;

/**
 * This object makes ssl encryption/decryption available to wand
 * so that passwords can be saved and recovered.
 *
 * Usage is to create a WandSecurityWrapper (for instance on the stack) and
 * then call Enable on it. If Enable returns OpStatus::OK, then ssl and passwords
 * and all that will be available until either Disable or the destructor is run.
 * In fact, it might be available even after Disable in case another
 * WandSecurityWrapper object is active.
 *
 * The complicated case is if Enable returns OpStatus::ERR_YIELD which is to be
 * interpreted as "are going to enable security but we will have to wait
 * for the user to input the master password first".
 *
 * If that happens, it's not much to do but to return to the main loop after registering
 * something in SetSuspendedOperation. That will make sure we can resume once the
 * master password is enabled. The code that will be triggered is WandManager::HandleCallback
 * with the message MSG_WAND_RESUME_OPERATION. The SuspendedOperation code will keep
 * a WandSecurityWrapper object active meanwhile so it's possible to let the original
 * WandSecurityWrapper die.
 *
 * @author Daniel Bratell
 */
class WandSecurityWrapper
{
public:
	/**
	 * Constructor. Will not make anything happen. Might be kept on the stack for simplicity.
	 */
	WandSecurityWrapper() : m_enabled(FALSE)
	{}
	/**
	 * See the class description for usage.
	 *
	 * @param[in] window The window which will be parent to any dialogs.
	 *
	 * @param[in] force_master_password If the master password
	 * security should be enabled even though it would normally not be
	 * needed. Typically needed when the database is about to be
	 * encrypted. See WandManager::UpdateSecurityState.
	 *
	 * @returns OpStatus::OK is security (ssl/password access) is
	 * enabled, OpStatus::ERR_YIELD if security will be enabled once
	 * the user has entered the correct password.  Any other error
	 * code in case something went very wrong (oom).
	 */
	OP_STATUS Enable(Window* window, BOOL force_master_password=FALSE);

	/**
	 *
	 * @param[in] force_master_password If the master password
	 * security should be enabled even though it would normally not be
	 * needed. Typically needed when the database is about to be
	 * encrypted. See WandManager::UpdateSecurityState.
	 *
	 * This is like Enable but for use only when there is no Window
	 * object. Any dialogs will get a random or no position/parent.
	 */
	OP_STATUS EnableWithoutWindow(BOOL force_master_password=FALSE);

	/**
	 * Disables the ssl code. Will only disable ssl/password access for real if this is the
	 * last active WandSecurityWrapper object.
	 */
	void Disable();

	/**
	 * Destructor.
	 */
	~WandSecurityWrapper();

private:
	OP_STATUS EnableInternal(OpWindow* platform_window, BOOL force_master_password);

	BOOL m_enabled;
};

#endif // !WANDSECURITYWRAPPER_H
