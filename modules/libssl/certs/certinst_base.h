/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef SSL_CERTINST_BASE_H
#define SSL_CERTINST_BASE_H

#if defined _NATIVE_SSL_SUPPORT_ && defined USE_SSL_CERTINSTALLER
#include "modules/libssl/options/optenums.h"

/** This structure contain the installation flags to be used: the certificate store and (if relevant) the validation flags */
struct SSL_Certificate_Installer_flags {
	SSL_CertificateStore store;
	BOOL warn_before_use;
	BOOL forbid_use;

	SSL_Certificate_Installer_flags(): store(SSL_Unknown_Store), warn_before_use(TRUE), forbid_use(TRUE){}
	SSL_Certificate_Installer_flags(SSL_CertificateStore str, BOOL warn, BOOL deny): store(str), warn_before_use(warn), forbid_use(deny){}
};

/** This class is the public class for the Ceritficate installation API
 *	
 *	Objects that inherits this class must act as multistep procedures that return to the caller between each step.
 *	Each step must terminate within a reasonable time and MUST NOT use blocking UI requests.
 *
 *	Implementations controls the sequence, the formats supported and how to indicate when the operation is finished.
 */
class SSL_Certificate_Installer_Base
{
public:

	/** Constructor */
	SSL_Certificate_Installer_Base(){};

	/** Destructor */
	virtual ~SSL_Certificate_Installer_Base(){};

	/** Starts the installation.
	 *	Returns:
	 *		InstallerStatus::INSTALL_FINISHED  when finished
	 *		InstallerStatus::ERR_INSTALL_FAILED when it fails
	 *		InstallerStatus::ERR_PASSWORD_NEEDED when a password is needed before installation can continue (password is managed internally)
	 */
	virtual OP_STATUS StartInstallation()=0;

	/** Return TRUE if the installation procedure is finished */
	virtual BOOL Finished()=0;

	/** Return TRUE if the installation was finsihed with success. The return is only valid if Finished() returns TRUE */
	virtual BOOL InstallSuccess()=0;

	/** Return the first remaining item of the warnings and errors encountered during the installation 
	 *	Str::S_NOT_A_STRING is returned if there are no more warnings. 
	 *	The return is only valid if Finished() returns TRUE
	 */
	virtual Str::LocaleString ErrorStrings(OpString &info)=0;

	/** Sets an optional password to use for certificate installation, only needed for non-interactive use.
	*
	* @param pass the password to use for import operations.
	* @return an appropriate return code from the OP_STATUS enumeration.
	*/
	virtual OP_STATUS SetImportPassword(const char* pass)=0;

};

#endif // USE_SSL_CERTINSTALLER

#endif // SSL_CERTINST_BASE_H
