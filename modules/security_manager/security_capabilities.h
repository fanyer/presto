/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef SECURITY_MANAGER_CAPABILITIES_H
#define SECURITY_MANAGER_CAPABILITIES_H

/** Security Manager */

// One capability for each operation defined in OpSecurityManager::Operation.
// The capabilities have the names of the operations as suffixes of their names.

#define SECMAN_CAP_DOM_STANDARD
#define SECMAN_CAP_DOM_LOADSAVE
#define SECMAN_CAP_PLUGIN_RUNSCRIPT
#define SECMAN_CAP_DOC_SET_PREFERRED_CHARSET

#define SECMAN_CAP_SECURITYSTATE
	// 2006-05-22: OpSecurityState exists, along with the API that takes it as an argument

// Module provides OpSecurityUtilities::IsSafeToExport
#define SECMAN_CAP_ISSAFETOEXPORT

// Module supports the operations XSLT_IMPORT_OR_INCLUDE and XSLT_DOCUMENT
#define SECMAN_CAP_XSLT_SECURITY

// ParseGadgetPolicy will not enter or leave the <security> element if the gadgets
// module does (defined by GADGETS_CAP_GADGETS_ENTERS_SEC_ELEMENT
#define SECMAN_CAP_GADGETS_ENTERS_SEC_ELEMENT

// Supports OpSecurityManager::PrivilegedBlock for privileged operations in selftests.
#define SECMAN_CAP_PRIVILEGED_BLOCK

// Security context created using gadget object
#define SECMAN_CAP_CTXT_CTOR_GADGETS

// Gadgets use new widget security spec (v2)
#define SECMAN_CAP_GADGETS_SEC_SPEC_20

// Security Manager has gadgets specific SetDefaultGadgetPolicy(OpGadgetClass *) function
#define SECMAN_CAP_GADGETS_SETDEFAULTGADGETPOLICY

// Module supports the operation DOM_ALLOWED_TO_NAVIGATE
#define SECMAN_CAP_DOM_ALLOWED_TO_NAVIGATE

// There is public domain list support in security_manager
#define SECMAN_CAP_PUBLIC_DOMAIN_LIST

// Public domain support indicate if there was enough data
#define SECMAN_CAP_PUBDOMAIN_DATA_INDICATION

// Module has OpSecurityManager::AllowUniteURL
#define SECMAN_CAP_ALLOW_UNITE_URL

// OpSecurityContext_Unite with DocumentManager member exists
#define SECMAN_CAP_UNITE_TAKES_DOCMAN

#endif // SECURITY_MANAGER_CAPABILITIES_H
