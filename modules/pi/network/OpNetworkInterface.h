/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef OPNETWORKINTERFACE_H
#define OPNETWORKINTERFACE_H

#ifdef PI_NETWORK_INTERFACE_MANAGER

class OpSocketAddress;

/** @short Network interface status. */
enum OpNetworkInterfaceStatus
{
	/** @short The network link (connection) is down.
	 *
	 * This means that the interface may not be used to reach remote hosts.
	 */
	NETWORK_LINK_DOWN,

	/** @short The network link (connection) is up.
	 *
	 * This means that the interface may be used to reach remote hosts.
	 */
	NETWORK_LINK_UP
};

/** @short Network interface. */
class OpNetworkInterface
{
public:
	virtual ~OpNetworkInterface() { }

	/** @short Get the address associated with this network interface.
	 *
	 * To determine the type of address (localhost/loopback, private, public),
	 * GetNetType() may be called on the object returned.
	 *
	 * @param[out] address Set to the local address associated with this
	 * interface. May set to an empty address if the network link is down (so
	 * that address->IsHostValid() would return FALSE, if called). The port
	 * number part of the OpSocketAddress is not relevant.
	 *
	 * @return OK or ERR_NO_MEMORY.
	 */
	virtual OP_STATUS GetAddress(OpSocketAddress* address) = 0;

	/** @short Get the current status of this network interface.
	 *
	 * @return Whether the network link is up or not.
	 */
	virtual OpNetworkInterfaceStatus GetStatus() = 0;

#ifdef PI_NETWORK_INTERFACE_INSPECTION

	/** @short Enum for different network connection types
	 *
	 */
	enum NetworkType
	{
		UNKNOWN,			//< any connection of a type not specified by this enum or impossible to detect the type.
		ETHERNET,			//< wired ethernet connection
		WIFI,				//< WiFi connection
		CELLULAR_RADIO,		//< any cellular telphony connection. To get more info of what is the actual connection type (gsm, cdma etc) @see OpTelephonyNetworkInfo
		IRDA,				//< infrared connection
		BLUETOOTH,			//< bluetooth connection
		DIRECT_PC_LINK		//< (e.g. ActiveSync)
	};

	/** @short Type of physical connection used by this interface.
	 */
	virtual NetworkType GetNetworkType() = 0;

	/** @short Gets system identifier of the interface. eg "eth0"
	 *
	 * @param[out] id Set to the name of the interface. MUST not be NULL.
	 * @return
	 *		- OK
	 * 		- ERR_NOT_SUPPORTED - if the platform doesn't support this method
	 * 		- ERR_NO_MEMORY - if OOM occured
	 * 		- ERR - any other error.
	 */
	virtual OP_STATUS GetId(OpString* id) = 0;

	/** @short Gets the APN of the network interface.
	 *
	 * This method will succeed only for network types which support APN(CELLULAR_RADIO).
	 *
	 * @param[out] apn Set to the name of the interface. MUST not be NULL.
	 * @return
	 *		- OK
	 * 		- ERR_NO_SUCH_RESOURCE - if the interface doesn't have APN
	 * 		- ERR_NOT_SUPPORTED - if the platform doesn't support this method
	 * 		- ERR_NO_MEMORY - if OOM occured
	 * 		- ERR - any other error.
	 */
	virtual OP_STATUS GetAPN(OpString* apn) = 0;

	/** @short Gets the SSID of used by the network interface.
	 *
	 * This method will succeed only for network types which support SSID(WIFI).
	 *
	 * @param[out] ssid Set to the name of the interface. MUST not be NULL.
	 * @return
	 *		- OK
	 * 		- ERR_NO_SUCH_RESOURCE - if the interface doesn't have SSID
	 * 		- ERR_NOT_SUPPORTED - if the platform doesn't support this method
	 * 		- ERR_NO_MEMORY - if OOM occured
	 * 		- ERR - any other error.
	 */
	virtual OP_STATUS GetSSID(OpString* ssid) = 0;
#endif //PI_NETWORK_INTERFACE_INSPECTION

};

/** @short Network interface manager.
 *
 * It is implemented on the platform side. Methods are called from core.
 *
 * This class is typically used as a singleton object.
 *
 * This class owns OpNetworkInterface instances, and maintains a list of
 * them. When a network interface is added or removed on the system, this will
 * be reflected in the list, and the listener (if set) will be notified. The
 * listener will also be notified when something about an interface changes
 * (such as address change, or link going up or down).
 *
 * The list of OpNetworkInterface instances must not change while the
 * OpNetworkInterfaceManager is locked. The interfaces themselves (their state,
 * address, etc.) are allowed to change while the manager is locked, as long as
 * the list remains the same. It is locked by calling BeginEnumeration(), and
 * unlocked again with EndEnumeration().
 */
class OpNetworkInterfaceManager
{
public:
	/** @short Create a new OpNetworkInterfaceManager object.
	 *
	 * @param[out] new_object Set to the new OpNetworkInterfaceManager object
	 * created. The caller must ignore this value unless OpStatus::OK is
	 * returned.
	 * @param listener Listener for network interface changes. May be
	 * NULL. Only interface additions and removals that take place after the
	 * OpNetworkInterfaceManager has been created shall be reported via the
	 * listener (i.e. don't report interfaces that already exist)
	 *
	 * @return ERR_NO_MEMORY on OOM, OK otherwise.
	 */
	static OP_STATUS Create(OpNetworkInterfaceManager** new_object, class OpNetworkInterfaceListener* listener);

	/** @short Destructor.
	 *
	 * The manager will be automatically unlocked here, and all the
	 * OpNetworkInterface objects associated with this
	 * OpNetworkInterfaceManager will be deleted, but that will not trigger
	 * calls listener-OnInterfaceRemoved() in this case.
	 */
	virtual ~OpNetworkInterfaceManager() { }

	/** @short Change the listener for this network interface manager.
	 *
	 * @param listener New listener to set. NULL is allowed.
	 */
	virtual void SetListener(OpNetworkInterfaceListener* listener) = 0;

	/** @short Begin network interface enumeration. Lock the manager.
	 *
	 * While the manager is locked, the method GetNextInterface() may be
	 * called, to list each interface. The first call to GetNextInterface()
	 * will return the first interface, the second call will return the second
	 * interface, and so on, until NULL is returned when there are no more
	 * interfaces in the list. While the manager is locked, the list of
	 * interfaces may not change. An interface's state is allowed to change,
	 * though.
	 *
	 * The manager may be unlocked again by calling EndEnumeration().
	 *
	 * Calling BeginEnumeration() when the manager is already locked will fail
	 * with OpStatus::ERR.
	 *
	 * @return OK, ERR or ERR_NO_MEMORY. Any other value than OK means that
	 * locking failed (so that calling GetNextInterface() will return NULL).
	 */
	virtual OP_STATUS BeginEnumeration() = 0;

	/** @short End network interface enumeration. Unlock the manager.
	 *
	 * Unlocks the manager after a previous call to BeginEnumeration().
	 *
	 * Calling EndEnumeration() when the manager is not locked has is allowed,
	 * but has no effect.
	 */
	virtual void EndEnumeration() = 0;

	/** @short Get the next interface in the list.
	 *
	 * May only be called while the manager is locked. See
	 * BeginEnumeration(). The first call to GetNextInterface() after a
	 * successful call to BeginEnumeration() will return the first interface in
	 * the list, the second call will return the second interface in the list,
	 * and so on, until NULL is returned when there are no more interfaces in
	 * the list.
	 *
	 * @return The next network interface in the list, or NULL if there are no
	 * more interfaces in the list. NULL is also returned if the manager is not
	 * locked.
	 */
	virtual OpNetworkInterface* GetNextInterface() = 0;
};

/** @short Listener for network interface related changes.
 *
 * Implemented in core. Methods are called from the platform side.
 */
class OpNetworkInterfaceListener
{
public:
	virtual ~OpNetworkInterfaceListener() { }

	/** @short A new network interface was added.
	 *
	 * The implementation of this method may want to call methods on the
	 * OpNetworkInterface to get the current status and address. That may be
	 * done synchronously from this method.
	 *
	 * @param nic The new interface that was added. The object is owned
	 * by the platform side. Core may use the object until the
	 * OpNetworkInterfaceManager is deleted, or OnInterfaceRemoved() is called
	 * on this interface, whichever comes first.
	 */
	virtual void OnInterfaceAdded(OpNetworkInterface* nic) = 0;

	/** @short A network interface was removed.
	 *
	 * @param nic The interface that was removed. This object is about to
	 * be deleted. Core must not use it during or after this call.
	 */
	virtual void OnInterfaceRemoved(OpNetworkInterface* nic) = 0;

	/** @short The status and/or address of a network interface changed.
	 *
	 * The implementation of this method should call methods on the
	 * OpNetworkInterface to see what changed. That may be done synchronously
	 * from this method.
	 *
	 * @param nic The interface whose status changed.
	 */
    virtual void OnInterfaceChanged(OpNetworkInterface* nic) = 0;
};

#endif // PI_NETWORK_INTERFACE_MANAGER

#endif // OPNETWORKINTERFACE_H
