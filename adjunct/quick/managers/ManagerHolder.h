/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef MANAGER_HOLDER_H
#define MANAGER_HOLDER_H

class GenericDesktopManager;


/** @brief A class that keeps track of managers (singletons)
  * An object of this type is owned by Application.
  */
class ManagerHolder
{
public:
	/**
	 * Constants for manager types.  The order imposed by the values of the
	 * enumerators is the order that the managers are initialized in
	 * ManagerHolder::InitializeManagers().
	 *
	 * Inter-manager dependencies are expressed by combining (OR-ing) the
	 * appropriate constants, and respected by ManagerHolder.
	 */
	enum ManagerType
	{
		TYPE_FEATURE			= (1 << 0),
#ifdef VEGA_OPPAINTER_SUPPORT
		TYPE_QUICK_ANIMATION	= (1 << 1),
#endif // VEGA_OPPAINTER_SUPPORT
		TYPE_SECURITY_UI		= (1 << 2),
		TYPE_OPERA_ACCOUNT		= (1 << 3),
		TYPE_NOTIFICATION		= (1 << 4),
		TYPE_WEBMAIL			= (1 << 5),
		TYPE_FAV_ICON			= (1 << 6) | TYPE_WEBMAIL,
#ifdef SUPPORT_SPEED_DIAL
		TYPE_SPEED_DIAL			= (1 << 7) | TYPE_FAV_ICON,
#endif // SUPPORT_SPEED_DIAL
#ifdef GADGET_SUPPORT
		TYPE_GADGET				= (1 << 8),
#endif // GADGET_SUPPORT
		TYPE_HOTLIST			= (1 << 9) | TYPE_FAV_ICON | TYPE_GADGET,
#ifdef WEBSERVER_SUPPORT
		TYPE_WEB_SERVER			= (1 << 10) | TYPE_OPERA_ACCOUNT | TYPE_GADGET | TYPE_HOTLIST,
#endif // WEBSERVER_SUPPORT
		TYPE_HISTORY			= (1 << 11) | TYPE_FAV_ICON | TYPE_HOTLIST,
		TYPE_COOKIE				= (1 << 12),
		TYPE_SECURITY			= (1 << 13),
#ifdef AUTO_UPDATE_SUPPORT
		TYPE_AUTO_UPDATE		= (1 << 14),
#endif // AUTO_UPDATE_SUPPORT
#ifdef SUPPORT_DATA_SYNC
		TYPE_SYNC				= (1 << 15),
#endif // SUPPORT_DATA_SYNC
		TYPE_BOOKMARK			= (1 << 16),
#ifdef M2_SUPPORT
		TYPE_MAIL				= (1 << 17) | TYPE_HOTLIST,
#endif // M2_SUPPORT
#ifdef WEB_TURBO_MODE
		TYPE_OPERA_TURBO		= (1 << 18),
#endif // WEB_TURBO_MODE
#ifdef OPERA_CONSOLE
		TYPE_CONSOLE			= (1 << 19),
#endif // OPERA_CONSOLE
		TYPE_TRANSFER			= (1 << 20),
		TYPE_SESSION_AUTO_SAVE	= (1 << 21),
		TYPE_EXTERNAL_DOWNLOAD_MANAGER = (1 << 22),
		TYPE_TOOLBAR			= (1 << 23) | TYPE_HOTLIST,
#ifdef VEGA_OPPAINTER_SUPPORT
        TYPE_MOUSE_GESTURE_UI   = (1 << 24) | TYPE_QUICK_ANIMATION,
#endif // VEGA_OPPAINTER_SUPPORT
		TYPE_EXTENSIONS	= (1 << 25) | TYPE_GADGET | TYPE_NOTIFICATION | TYPE_SPEED_DIAL,
		TYPE_TIPS	= (1 << 26),
#ifdef PLUGIN_AUTO_INSTALL
		TYPE_PLUGIN_INSTALL		= (1 << 27) | TYPE_AUTO_UPDATE,
#endif // PLUGIN_AUTO_INSTALL
		TYPE_REDIRECTION		= (1 << 28) | TYPE_BOOKMARK | TYPE_SPEED_DIAL | TYPE_HOTLIST,
		TYPE_CLIPBOARD			= (1 << 29),
		TYPE_ALL				= 0xFFFFFFFF
	};

	ManagerHolder() : m_destroy_managers(false) {}
	~ManagerHolder();

	/**
	 * Initializes managers.
	 *
	 * @param type_mask a combination of ManagerType values specifying the set
	 *		of managers to initialize.  By default, all managers are
	 *		initialized.
	 */
	OP_STATUS InitManagers(INT32 type_mask = TYPE_ALL);

private:
	typedef GenericDesktopManager* (*ManagerCreator)();
	typedef void (*ManagerDestroyer)();

	struct ManagerDescr
	{
		ManagerDescr(ManagerType _type, ManagerCreator _creator, ManagerDestroyer _destroyer, const char* _name)
			: type(_type), creator(_creator), destroyer(_destroyer), name(_name) {}

		ManagerType type;
		ManagerCreator creator;
		ManagerDestroyer destroyer;
		const char *name;
	};

	template<class T> static ManagerDescr CreateManagerDescr(ManagerType type, const char* name)
		{ return ManagerDescr(type, T::CreateInstance, T::DestroyInstance, name); }

	static int ManagerDescrLessThan(const void* lhs, const void* rhs);

	/**
	 * A table of manager type "descriptors".  Each descriptor maps a manager
	 * type constant into a C++ type.
	 */
	static ManagerDescr s_manager_descrs[];

	/**
	 * This currently only here to be able to test DesktopBootstrap, i.e., create
	 * a DesktopBootstrap instance that doesn't destroy managers.
	 */
	bool m_destroy_managers;
};

#endif // MANAGER_HOLDER_H
