/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef DESKTOP_MANAGER_H
#define DESKTOP_MANAGER_H

/** If you want to create a manager (singleton):
  *   1) Derive YourManager from DesktopManager<YourManager>, e.g.
  *
  *        class YourManager : public DesktopManager<YourManager>
  *        {
  *            virtual OP_STATUS Init();
  *        };
  *
  *   2) Define a new manager type TYPE_YOUR_MANAGER in
  *      ManagerHolder::ManagerType.
  *   3) Add YourManager to ManagerHolder::s_manager_descrs in
  *      ManagerHolder.cpp.
  *
  * You can then retrieve YourManager with YourManager::GetInstance(), provided
  * it has already been initialized via ManagerHolder.
  *
  * I.e., OpBootManager uses a ManagerHolder to initialize all the managers at
  * the start of OpBootManager::StartBrowser(), and destroys them together with
  * the ManagerHolder object.
  */
class GenericDesktopManager
{
public:
	virtual ~GenericDesktopManager() {}

	/** Initialize manager, any memory allocation should be done here
	  * g_application is guaranteed to exist when this function is called
	  */
	virtual OP_STATUS Init() = 0;
};


template<class ManagerName>
class DesktopManager : public GenericDesktopManager
{
public:
	/** @return singleton instance of this manager
	  */
	static ManagerName* GetInstance() { return s_instance; }
	virtual ~DesktopManager() {
#ifdef SELFTEST
		if (this == s_instance) // Prevent fake managers crash opera
#endif
		s_instance = NULL; 
	}

private:
	friend class ManagerHolder;

	static GenericDesktopManager* CreateInstance()
	{
		if (!s_instance)
			s_instance = OP_NEW(ManagerName, ());
		return s_instance;
	}

	static void DestroyInstance()
	{
		OP_DELETE(s_instance);
		s_instance = 0;
	}

	static ManagerName* s_instance;
};


template<class ManagerName>
ManagerName* DesktopManager<ManagerName>::s_instance = 0;


#endif // DESKTOP_MANAGER_H

