/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef WINDOWSUACOMPONENTMANAGER_H
# define WINDOWSUACOMPONENTMANAGER_H

# ifdef WIN_OK_UACOMPONENTMANAGER
#  include "modules/pi/OpUAComponentManager.h"

/** Basic implementation of OpUAComponentManager.
 *
 * This class provides GetOSString, because that's what it makes sense to
 * implement using Win32 APIs.  The optional APIs of this class (see
 * TWEAK_URL_UA_COMPONENTS_CONTEXT) relate to project-specific choices.
 *
 * You also need to provide an implementation of
 * OpUAComponentManager::Create() along the lines of this:
 *
 * @code
 *  OP_STATUS OpUAComponentManager::Create(OpUAComponentManager **target_pp)
 *  {
 *   OpAutoPtr<WindowsUAComponentManager> manager(OP_NEW(WindowsUAComponentManager, ()));
 *   if (!manager.get())
 *    return OpStatus::ERR_NO_MEMORY;
 *   RETURN_IF_ERROR(manager->Construct());
 *   *target_pp = manager.release();
 *   return OpStatus::OK;
 *  }
 * @endcode
 *
 * An instance of this class requires second-stage construction: call its
 * Construct() method and check the status it returns.
 */
class WindowsUAComponentManager : public OpUAComponentManager
{
protected:
	friend class OpUAComponentManager; // for its Create()
	/** Constructor.
	 *
	 * Only for use by OpComponentUAManager::Create() or a derived class's
	 * constructor.
	 */
	WindowsUAComponentManager() { }

	/** Set-up.
	 *
	 * Must be called by OpComponentUAManager::Create(), possibly via derived
	 * class over-riding this method. Derived classes that do so must call
	 * this.
	 *
	 * @return OK on success, else error value indicating problem.
	 */
	virtual OP_STATUS Construct();

public:
	virtual const char *GetOSString(Args &)
	{ return m_os_ver.CStr(); }

protected:
	/** True identification of operating system.
	 *
	 * Initialized by constructor; for use in user-agent strings, via
	 * GetOSString().  Includes hardware type and, where easy, version information.
	 */
	OpString8 m_os_ver;
};
# endif // WIN_OK_UACOMPONENTMANAGER
#endif // WINDOWSUACOMPONENTMANAGER_H
