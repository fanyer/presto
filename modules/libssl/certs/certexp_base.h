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

#ifndef SSL_CERTEXP_BASE_H
#define SSL_CERTEXP_BASE_H

#if defined _NATIVE_SSL_SUPPORT_ && defined USE_SSL_CERTINSTALLER

/** This class is the public class for the Ceritficate export API
 *	
 *	Objects that inherits this class must act as multistep procedures that return to the caller between each step.
 *	Each step must terminate within a reasonable time and MUST NOT use blocking UI requests.
 *
 *	Implementations controls the sequence, the formats supported and how to indicate when the operation is finished.
 */
class SSL_Certificate_Export_Base
{
public:

	/** Constructor */
	SSL_Certificate_Export_Base(){};

	/** Destructor */
	virtual ~SSL_Certificate_Export_Base(){};

	/** Starts the export.
	 *	Returns:
	 *		InstallerStatus::INSTALL_FINISHED  when finished
	 *		InstallerStatus::ERR_INSTALL_FAILED when it fails
	 *		InstallerStatus::ERR_PASSWORD_NEEDED when a password is needed before installation can continue (password is managed internally)
	 */
	virtual OP_STATUS StartExport()=0;

	/** Return TRUE if the export procedure is finished */
	virtual BOOL Finished()=0;

	/** Return TRUE if the export was finished with success. The return is only valid if Finished() returns TRUE */
	virtual BOOL ExportSuccess()=0;
};

#endif // USE_SSL_CERTINSTALLER

#endif // SSL_CERTEXP_BASE_H
