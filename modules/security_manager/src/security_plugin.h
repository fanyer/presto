/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

/*
   Cross-module security manager: Plugin model
   Lars T Hansen.

   See documentation/plugin-security.txt in the documentation directory.  */

#ifndef SECURITY_PLUGIN_H
#define SECURITY_PLUGIN_H

class OpSecurityContext;
class OpRegExp;

class OpSecurityContext_Plugin
{
#ifdef _PLUGIN_SUPPORT_

	/* Nothing right now */

#endif // _PLUGIN_SUPPORT_
};

class OpSecurityManager_Plugin
{
#ifdef _PLUGIN_SUPPORT_

public:
	OpSecurityManager_Plugin();
	virtual ~OpSecurityManager_Plugin();

protected:
	OP_STATUS CheckPluginSecurityRunscript(const OpSecurityContext& source, const OpSecurityContext& target, BOOL& allowed);
		/**< Check that the source context can run a specific script in the target context.
			 The target context must include the source code for the script.  */

private:
	OP_STATUS Init();

	OpRegExp* re;
#endif // _PLUGIN_SUPPORT_
};

#endif // SECURITY_PLUGIN_H
