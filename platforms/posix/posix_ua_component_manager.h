/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef POSIX_UA_COMPONENT_MANAGER_H
# define POSIX_UA_COMPONENT_MANAGER_H __FILE__

# ifdef POSIX_OK_HOST
#  include "modules/pi/OpUAComponentManager.h"

/** Basic implementation of OpUAComponentManager.
 *
 * This class provides GetOSString, because that's what it makes sense to
 * implement using POSIX APIs.  The optional APIs of this class (see
 * TWEAK_URL_UA_COMPONENTS_CONTEXT) relate to project-specific choices.
 *
 * You also need to provide an implementation of
 * OpUAComponentManager::Create() along the lines of this:
 *
 * @code
 *  OP_STATUS OpUAComponentManager::Create(OpUAComponentManager **target_pp)
 *  {
 *   OpAutoPtr<PosixUAComponentManager> manager(OP_NEW(PosixUAComponentManager, ()));
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
class PosixUAComponentManager :
	public OpUAComponentManager
{
protected:
	friend class OpUAComponentManager; // for its Create()
	/** Constructor.
	 *
	 * Only for use by OpComponentUAManager::Create() or a derived class's
	 * constructor.
	 */
	PosixUAComponentManager();

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
	virtual const char *GetOSString(Args &args)
	{
		OP_ASSERT(m_os_ver.CStr() || !"Did you neglect to call Construct() ?");
		return (args.ua == UA_MSIE_Only) ? m_ms_ie_ver : m_os_ver.CStr();
	}

private:
	/** OS to claim we're using, when claiming to be MSIE.
	 *
	 * Initialized by constructor; for use in user-agent strings, via
	 * GetOSString().
	 */
	char m_ms_ie_ver[sizeof(POSIX_IE_SYSNAME)];	// ARRAY OK 2011-10-07 peter

	/** True identification of operating system.
	 *
	 * Initialized by constructor; for use in user-agent strings, via
	 * GetOSString().  Includes hardware type and, where easy, version information.
	 */
	OpString8 m_os_ver;
};

# endif // POSIX_OK_HOST
#endif // POSIX_UA_COMPONENT_MANAGER_H
