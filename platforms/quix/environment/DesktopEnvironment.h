/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Karianne Ekern (karie)
 */

#ifndef QUIX_DESKTOP_ENVIRONMENT
#define QUIX_DESKTOP_ENVIRONMENT

/** 
 * @brief 
 *
 * DesktopEnvironment
 *
 */
class DesktopEnvironment
{
public:
	enum ToolkitType
	{
		TOOLKIT_AUTOMATIC,
		TOOLKIT_QT,
		TOOLKIT_GTK,
		TOOLKIT_KDE,
		TOOLKIT_X11,
		TOOLKIT_GTK3,
		TOOLKIT_UNKNOWN
	};

	enum ToolkitEnvironment
	{
		ENVIRONMENT_GNOME,
		ENVIRONMENT_GNOME_3,
		ENVIRONMENT_UNITY,
		ENVIRONMENT_KDE,
		ENVIRONMENT_XFCE,
		ENVIRONMENT_UNKNOWN,
		ENVIRONMENT_TO_BE_DETERMINED
	};

	static DesktopEnvironment& GetInstance();

	/**
	  * @return Desktop environment the user is running
	  */
	ToolkitEnvironment GetToolkitEnvironment();

	void GetDesktopEnvironmentName(OpString8& name);

	/**
	  * @return the preferred ToolkitType to use
	  *         this can be forced by pref Dialog Toolkit
	  *         and may be different from the ToolkitEnvironment
	  *         Will always return a concrete toolkit type (ie. never TOOLKIT_AUTOMATIC or TOOLKIT_UNKNOWN)
	  */
	ToolkitType GetPreferredToolkit();

private:
	void DetermineToolkitAutomatically();
	bool IsGnomeRunning();
	bool IsGnome3Running();
	bool IsUbuntuUnityRunning();
	bool IsKDERunning();
	bool IsXfceRunning();

	ToolkitType m_toolkit;
	ToolkitEnvironment m_toolkit_environment;
	bool m_initialized;

	DesktopEnvironment();
	~DesktopEnvironment() {}
};

#endif // QUIX_DESKTOP_ENVIRONMENT
