/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef UPNP_CAPABILITIES_H
#define UPNP_CAPABILITIES_H


#define UPNP_MODULE_NAME alien_scout
#define UPNP_MODULE_VERSION 1
#define UPNP_MODULE_FINAL_VERSION 1

// The UPnP module is included
#define UPnP_CAP_MODULE

// Second version of UPnPListener 
#define UPNP_CAP_LISTENER_V2
// Working version of Self delete
#define UPNP_CAP_SELFDELETE2

// UPnPListener has been split in UPnPListener and UPnPPortListener
#define UPNP_CAP_PORT_LISTENER_INTERFACE

// UPnPDevice::GetTrust() is available
#define UPNP_CAP_TRUST 

#endif // UPNP_CAPABILITIES_H
