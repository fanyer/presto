// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//

#ifndef UNIX_OPAUTOUPDATE_H
#define UNIX_OPAUTOUPDATE_H

#ifdef AUTO_UPDATE_SUPPORT

#include "adjunct/autoupdate/pi/opautoupdatepi.h"

class UnixOpAutoUpdate : public OpAutoUpdatePI
{
public:
     ~UnixOpAutoUpdate() {}
	
     static OpAutoUpdatePI* Create();

     /*
      * Get OS name
      *
      * Name of the operating system as will be passed to the auto update 
      * system. It should be of the form as show on the download page
      * i.e. http://www.opera.com/download/index.dml?custom=yes
      *
      * @param os (in/out) Name of the OS
      *
      * Example of values: FreeBSD, Linux, MacOS, Windows
      *
      * @return OP_STATUS Returns OpStatus::OK on success, otherwise OpStatus::ERR.
      */
     OP_STATUS GetOSName(OpString& os);

     /*
      * Get OS version
      *
      * Version of the operating system as will be passed to the auto update system
      *
      * @param version (in/out) Name of OS version
      *
      * Example of values: 10.2, 10.3, 7, 95
      *
      * @return OP_STATUS Returns OpStatus::OK on success, otherwise OpStatus::ERR.
      */
     OP_STATUS GetOSVersion(OpString& version);

     /*
      * Get architecture
      *
      * @param arch (in/out) Name of architecture
      *
      * Example of values: i386, ppc, sparc, x86-64
      *	 
      * @return OP_STATUS Returns OpStatus::OK on success, otherwise OpStatus::ERR.
      */
     OP_STATUS GetArchitecture(OpString& arch);

     /*
      * Get package type (*nix only)
      *
      * @param package (in/out) Name of *nix package
      *
      * Example of values: deb, rpm, tar.gz, tar.bz2, tar.Z, pkg.gz, pkg.bz2, pkg.Z
      *
      * @return OP_STATUS Returns OpStatus::OK on success, otherwise OpStatus::ERR.
      *
      * Platforms other than *nix should set the string to empty and return OpStatus::OK.
      */
     OP_STATUS GetPackageType(OpString& package);

     /*
      * Get gcc version (*nix only)
      *
      * @param gcc (in/out) GCC version
      *
      * Example of values: gcc295, gcc3, gcc4
      *
      * @return OP_STATUS Returns OpStatus::OK on success, otherwise OpStatus::ERR.
      *
      * Platforms other than *nix should set the string to empty and return OpStatus::OK.
      */
     OP_STATUS GetGccVersion(OpString& gcc);

     /*
      * Get QT version (*nix only)
      *
      * @param qt (in/out) QT version
      *
      * Example of values: qt3-shared, qt3-static, qt4-unbundled, qt4-bundled
      *
      * @return OP_STATUS Returns OpStatus::OK on success, otherwise OpStatus::ERR.
      *
      * Platforms other than *nix should set the string to empty and return OpStatus::OK.
      */
     OP_STATUS GetQTVersion(OpString& qt);

	OP_STATUS ExtractInstallationFiles(uni_char *package) { return OpStatus::OK; }
	BOOL ExtractionInBackground() { return FALSE; }
};

#endif // AUTO_UPDATE_SUPPORT
#endif // UNIX_OPAUTOUPDATE_H
