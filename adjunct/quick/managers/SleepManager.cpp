/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#include "adjunct/quick/managers/SleepManager.h"
#include "adjunct/quick/managers/KioskManager.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"


/***********************************************************************************
 ** Get singleton instance
 **
 ** SleepManager::GetInstance
 ***********************************************************************************/
SleepManager& SleepManager::GetInstance()
{
	static SleepManager instance;
	return instance;
}


/***********************************************************************************
 ** Constructor
 **
 ** SleepManager::SleepManager
 ***********************************************************************************/
SleepManager::SleepManager()
  : m_stay_awake_listener(0)
  , m_stay_awake_sessions(0)
{
}


/***********************************************************************************
 ** Platform should call this function when system goes into sleep mode
 **
 ** SleepManager::Sleep
 ***********************************************************************************/
void SleepManager::Sleep()
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnSleep();
	}
}


/***********************************************************************************
 ** Platform should call this function when system comes out of sleep mode
 **
 ** SleepManager::WakeUp
 ***********************************************************************************/
void SleepManager::WakeUp()
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnWakeUp();
	}
}


/***********************************************************************************
 ** Check whether the system is allowed to sleep as far as Opera is concerned
 **
 ** SleepManager::SystemCanGoToSleep
 ** @return Whether Opera allows the system to go to sleep
 ***********************************************************************************/
BOOL SleepManager::SystemCanGoToSleep()
{
	return m_stay_awake_sessions == 0;
}


/***********************************************************************************
 ** Start a session in which the system should stay awake
 **
 ** SleepManager::StartStayAwakeSession
 ***********************************************************************************/
void SleepManager::StartStayAwakeSession()
{
	m_stay_awake_sessions++;

	if (m_stay_awake_listener && m_stay_awake_sessions == 1)
		m_stay_awake_listener->OnStayAwakeStatusChanged(TRUE);
}


/***********************************************************************************
 ** End a session in which the system should stay awake
 **
 ** SleepManager::EndStayAwakeSession
 ***********************************************************************************/
void SleepManager::EndStayAwakeSession()
{
	if (m_stay_awake_sessions > 0)
		m_stay_awake_sessions--;

	if (m_stay_awake_listener && m_stay_awake_sessions == 0)
		m_stay_awake_listener->OnStayAwakeStatusChanged(FALSE);
}
