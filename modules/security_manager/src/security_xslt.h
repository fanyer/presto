/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright 2007-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/** @file security_xslt.cpp
 * Cross-module security manager: XSLT model.
 *
 * See document/xslt.txt
 */

#ifndef SECURITY_XSLT_H
#define SECURITY_XSLT_H

class OpSecurityContext;

class OpSecurityManager_XSLT
{
protected:
	/**
	 * Check that the source context is allowed to access the target resource via XSL
	 * stylesheet import or via the document() function.
	 *
	 */
	OP_STATUS CheckXSLTSecurity(const OpSecurityContext& source, const OpSecurityContext& target, BOOL& allowed);
};

#endif // SECURITY_XSLT_H
