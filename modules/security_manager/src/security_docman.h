/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright 2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/** @file security_docman.cpp
 * Cross-module security manager: docman model.
 *
 * See document/xxx.txt
 */

#ifndef SECURITY_DOCMAN_H
#define SECURITY_DOCMAN_H

class OpSecurityCheckCallback;
class OpSecurityContext;
class OpSecurityState;

class OpSecurityContext_Docman
{
public:
	OpSecurityContext_Docman() : document_manager(NULL) {}
	BOOL IsTopDocument() const;
	DocumentManager *GetDocumentManager() const { return document_manager; };
protected:
	DocumentManager* document_manager;
};

class OpSecurityManager_Docman
{
protected:
	OP_STATUS CheckDocManUrlLoadingSecurity(const OpSecurityContext& source, const OpSecurityContext& target, OpSecurityCheckCallback* callback);
};

#endif // SECURITY_DOCMAN_H
