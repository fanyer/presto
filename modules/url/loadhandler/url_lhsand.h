/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
**
*/

#ifndef URLLOADHANDLER_MOUNTPOINT_H
#define URLLOADHANDLER_MOUNTPOINT_H

#include "modules/util/OpHashTable.h"

class MountPoint 
{
public:
	/* Creates a mountpoint which becomes available through the http protocol mountpoint://name/ 
 	 *
 	 * The mountpoint will be located on /mountpointFilePath/mountpointFileSubPath
 	 * 
 	 * mountpointFileSubPath will be resolved for all dangerous /../'s. So any path given by a user should be put into mountpointFileSubPath.
 	 * This way it is guarantied that the user will not get access to anything below /mountpointFilePath/.
	 *
 	 * ! The caller of this function is responsible for deleting the created mountpoint !  
  	 *
  	 * @param mountpoint (out) 	    The mountpoint to created. 
  	 * @param name				    The name of the mountpoint
  	 * @param windowId			    The window where the sandobox will be active
  	 * @param mountpointFilePath    The path of the mountpoint
  	 *  
 	 * @return 	OpBoolean::IS_TRUE 		If a mountpoint with same name AND windowId already exists. 
 	 * 			OpBoolean::IS_FALSE 	If mountpoint is created
 	 * 			OpStatus::ERR			If name or mountpointFileSubPath contains dangerous elements (using /../ constructions) 
 	 * 			OpStatus::ERR_NO_MEM 	If when out of memory*/
	static OP_BOOLEAN Make(MountPoint *&mountpoint, const OpStringC16 &name, unsigned int windowId, const OpStringC16 &mountpointFilePath);

	/* Get the window of this mountpoint */
	unsigned int GetWindowId() { return m_windowId; }
	
	/* Get the path on the local disk where this sandox is located*/
	const OpStringC16& GetMountPointFilePath() { return m_mountPointFilePath; }
	
	/* Get the name of this mountpoint */
	const OpStringC16& GetMountPointName() { return m_name; }

	virtual ~MountPoint();

private:
	MountPoint(unsigned int windowId);	
	
	unsigned int m_windowId;
	OpString m_mountPointFilePath;
	OpString m_name;
	
	friend class MountPointProtocolManager;
};



class MountPointProtocolManager
{
public:
	MountPointProtocolManager() {}

	/* Get the path for a given mountpoint 
	 * 
	 * @param name 		The name of the sandox
	 * @param window_id	The window where the mountpoint is located
	 * 
	 */
	const uni_char *GetMountPointPath(const OpStringC16 &name, unsigned int window_id);
	
	virtual ~MountPointProtocolManager() {}

private:
	void RemoveMountPoint(MountPoint *mountpoint);
	
	OP_STATUS AddMountPoint(MountPoint *mountpoint);
	
	OpVector<MountPoint > m_mountPoints;

	friend class MountPoint;
};

#endif // URLLOADHANDLER_MOUNTPOINT_H
