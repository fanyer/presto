/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

/** \file
 * This file declares the PlatformUpdater interface.
 *
 * @author Marius Blomli mariusab@opera.com
 */

#ifndef _PLATFORMUPDATER_H_INCLUDED_
#define _PLATFORMUPDATER_H_INCLUDED_

#ifdef AUTO_UPDATE_SUPPORT

/**
 * Interface to the platform used to manage updating of the 
 * installation. Implement this interface and supply an instance
 * of the subclass when calling AutoUpdater::Init to activate auto
 * updating on the platform.
 */
class PlatformUpdater
{
public:
	/**
	 * Virtual destructor.
	 */
	virtual ~PlatformUpdater() {}
	/**
	 * Creates a new PlatformUpdater instance. This function must be 
	 * implemented by the platform code. The implementation must 
	 * create an instance of the platform's implementation of 
	 * PlatformUpdater which is returned through the new_object parameter.
	 *
	 * The instance is owned by the calling code and must be deleted by the
	 * caller. (The desturctor is public and virtual.)
	 *
	 * @param new_instance pointer that the function sets to the newly 
	 * created instance.
	 *
	 * @return OK if the instance was successfully created, some error
	 * otherwise.
	 */
	static OP_STATUS Create(PlatformUpdater** new_instance);
	/**
	 * Call this function to check the integrity of a downloaded update 
	 * package. 
	 */
	virtual BOOL CheckUpdate(/* Supply the package in some way */) = 0;
	/**
	 * Call this function to initiate the update.
	 *
	 * @todo Think through the sequence, we can't have the platform code 
	 * just restarting opera or locking resources.
	 */
	virtual OP_STATUS InitiateUpdate(/* Supply the package in some way */) = 0;
	/**
	 * This function is used by the AutoUpdater to determine if automatic 
	 * updating is available to the user. (Depending on OS, user privileges,
	 * install location, ...)
	 */
	virtual BOOL HasInstallPrivileges() = 0;
};

// This class will move into platform code, and be reimplemented on the other platforms as autoupdate is added.
class WindowsUpdater : public PlatformUpdater
{
public:
	WindowsUpdater();
	~WindowsUpdater();
	BOOL CheckUpdate();
	OP_STATUS InitiateUpdate();
	BOOL HasInstallPrivileges();
};

#endif // AUTO_UPDATE_SUPPORT

#endif // _PLATFORMUPDATER_H_INCLUDED_
