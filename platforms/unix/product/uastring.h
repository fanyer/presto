/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2004 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
**
** Morten Stenshorne
*/

#ifndef UASTRING_H
#define UASTRING_H

#include "config.h"

#include <sys/utsname.h> // so that modules/softcore/ua.cpp compiles

# define UA_INCLUDE_ARCHITECTURE
# define UA_INCLUDE_WINDOWSYS
// # define UA_INCLUDE_SCREENSIZE

OP_STATUS GetUnixUaWindowingSystem(OpString8 &uastring);
OP_STATUS GetUnixUaScreenSize(OpString8 &uastring);
OP_STATUS GetUnixUaSystemString(OpString8 &uastring);
OP_STATUS GetUnixUaDeviceInfo(OpString8 &uastring);

#ifdef UA_INCLUDE_DEVICENAME
OP_STATUS GetUnixUaPrettyDeviceName(OpString8 &uastring);
#endif // UA_INCLUDE_DEVICENAME

#ifdef CUSTOM_UASTRING_SUPPORT
OP_STATUS GetUnixUaOSName(OpString8 &uastring);
OP_STATUS GetUnixUaOSVersion(OpString8 &uastring);
OP_STATUS GetUnixUaMachineArchitecture(OpString8 &uastring);
OP_STATUS GetUnixUaDeviceIdentifier(OpString8 &uastring);
OP_STATUS GetUnixUaDeviceSoftwareVersion(OpString8 &uastring);
#endif // CUSTOM_UASTRING_SUPPORT

#endif // UASTRING_H
