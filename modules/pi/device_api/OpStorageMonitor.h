/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_PI_OPSTORAGEMONITOR_H
#define MODULES_PI_OPSTORAGEMONITOR_H

#ifdef PI_STORAGE_MONITOR

/**
 * @short Enumerates types of a storage devices
 */
enum StorageType
{
	/** The type of storage is unknown. */
	STORAGE_UNKNOWN,
	/** The storage is a removable storage device, such as a USB flash drive or a SD card */
	STORAGE_REMOVABLE,
	/** The storage is a fixed drive */
	STORAGE_FIXED,
};

/**
 * @short Structure storing information about a storage device
 */
struct StorageInfo
{
	/** A storage device's mountpoint e.g. C: or /mnt/diskc. This must be unique among all storage devices (MANDATORY).
	 *  Root directory on Unix systems is a special case and may be identified by both "" and "/"
	 */
	const uni_char *mountpoint;
	/** Type of a storage device*/
	StorageType type;
	/** A storage device's label */
	const uni_char *label;
};

/**
 * Storage monitor class
 */
class OpStorageMonitor
{
public:
	virtual ~OpStorageMonitor() {}
	/** Create an OpStorageMonitor object.
	  *
	  * This object will be used by the core to retrieve
	  * information about present storage devices in the system.
	  *
	  * @param storage_monitor The newly created storage monitor object.
	  * @param listener The listener that the platform will inform about new storage devices
	  * @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM
	  */
	static OP_STATUS Create(OpStorageMonitor **drive_monitor, class OpStorageListener *listener);
	/* Starts Monitoring.
	 *
	 * The platform implementation is then expected to call the OpStorageListener::OnStorageMounted
	 * callback once for every storage device which is present in the system.
	 *
	 */
	virtual void StartMonitoring() = 0;
	/* Stops Monitoring.
	 *
	 * After this call the listener must not be notified (unless OpStorageMonitor::StartMonitoring() is called again).
	 *
	 */
	virtual void StopMonitoring() = 0;
	/* Gets amount of free space on a storage device identified by StorageInfo.mountpoint
	 *
	 * @param - info - StorageInfo structure allowing to identify the storage device.
	 *
	 * @return - storage's free size
	 */
	virtual UINT64 GetFreeSize(StorageInfo *info) = 0;
	/* Gets amount of total size of a storage device identified by StorageInfo.mountpoint
	 *
	 * @param - info - StorageInfo structure allowing to identify the storage device.
	 *
	 * @return - storage's total size
	 */
	virtual UINT64 GetTotalSize(StorageInfo *info) = 0;
};

 /** OpStorageListener
  *
  * This is the listener that is implemented by core.
  *
  * After the platform has received a StartMonitoring() call on OpStorageManager class
  * it has to call the OnStorageMounted function once for every storage device which
  * is present in the system. Later core must be also informed about
  * the storage devices added/removed via OnStorageMounted/OnStorageUnmounted accordingly
  * without additional call to StartMonitoring()
  */
class OpStorageListener
{
public:
	virtual ~OpStorageListener() {}
	/** Informs CORE that new storage device appeared in the system
	 *
	 * @param info - info about a new storage device. Core makes copy of this structure so
	 * platform remains the owner of it.
	 *
	 * @return OK if everything went OK,
	 * ERR in case of some error,
	 * ERR_NO_MEMORY in case of OOM
	 */
	virtual OP_STATUS OnStorageMounted(const StorageInfo &info) = 0;
	/** Informs CORE that some storage device disappeared from the system
	 *
	 * @param info - info about the storage device. Core makes copy of this structure so
	 * platform remains the owner of it.
	 *
	 * @return OK if everything went OK
	 * ERR_NO_SUCH_RESOURCE when storage with the given StorageInfo.name hasn't been mounted
	 */
	virtual OP_STATUS OnStorageUnmounted(const StorageInfo &info) = 0;
};

#endif // PI_STORAGE_MONITOR

#endif // MODULES_PI_OPSTORAGEMONITOR_H
