/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */
#ifndef SECURITY_CROSS_ORIGIN_H
#define SECURITY_CROSS_ORIGIN_H

#ifdef CORS_SUPPORT
/** OpCrossOrigin - Cross Origin Resource Sharing implementation.

    CORS interfaces:
    ----------------

    OpCrossOrigin_Manager:
         - implements the public CORS interface, having methods
           to handle the steps various resource sharing steps.
         - caches data from preflight requests, and manages
           the updates and invalidation of that ephemeral cache.

    OpCrossOrigin_Request:
         - CORS characterizes a cross-origin request via a handful
           of attributes; all kept together on the Request object.
           The request object is then updated as the resource sharing
           check advances towards the final check. Or, in case of
           failure, recording why it failed.

    Internal support classes:
    -------------------------

    OpCrossOrigin_Loader:
         - implements the loading a preflight request; an OPTIONS
           ping to the target resource to query if an actual CORS
           request would be supported. Only needed for "non simple"
           requests.

    OpCrossOrigin_PreflightCache:
         - a basic cache of preflight response information. Utilised
           to cut down on preflight request traffic. A CORS manager
           keeps a preflight cache.

    OpCrossOrigin_Utilities:
         - Utility methods.

That's the CORS interfaces and structure; this header file defines
the interfaces for integrating them into the OpSecurity* infrastructure,
i.e., OpSecurityState, OpSecurityContext and OpSecurityManager.
*/

class OpSecurityContext;
class OpSecurityState;
class OpSecurityCheckCallback;
class OpSecurityCheckCancel;

#include "modules/security_manager/src/cors/cors_config.h"
#include "modules/security_manager/src/cors/cors_request.h"
#include "modules/security_manager/src/cors/cors_manager.h"

class OpSecurityContext_CrossOrigin
{
public:
	OpCrossOrigin_Request *GetCrossOriginRequest() const { return cross_origin_request; }
	/**< If non-NULL, this security context is requesting cross-origin access.
	     The request object keeps security check information and outcomes of CORS
	     validation. */

protected:
	OpCrossOrigin_Request *cross_origin_request;
	/**< Non-NULL if access outside of this context's origin is requested.
	     If supplied, this value is assumed owned by the security context
	     and the security manager. */

	void Init() { cross_origin_request = NULL; }
};

/** The CORS/cross-origin interface is accessed indirectly through
    OpSecurityManager::CheckSecurity(), but with two exceptions:

       - deleting private/sensitive data can also invalidate the
         non-persistent preflight cache.

       - Network handles redirects "inline", issuing a
         redirected request directly. Provide an entry point
         for this code to verify that a cross-origin redirect
         is allowable by CORS. */
class OpSecurityManager_CrossOrigin
{
public:
	void ClearCrossOriginPreflightCache();
	/**< Purge the contents of the preflight cache. */

	static OP_BOOLEAN VerifyRedirect(URL &url, URL &moved_to);
	/**< Is the redirect from 'url' to 'moved_to' allowed by CORS? */

	OpCrossOrigin_Manager *GetCrossOriginManager(const URL &origin_url);
	/**< Return the cross-origin security manager associated with the origin of
	     the given URL. If none supported/allowed, returns NULL. */

protected:
	friend class OpSecurityManager_DOM_LoadSaveCallback;

	OpSecurityManager_CrossOrigin()
		: cross_origin_manager(NULL)
	{
	}

	virtual ~OpSecurityManager_CrossOrigin();

	OP_BOOLEAN CheckCrossOriginAccessible(const OpSecurityContext& source, const OpSecurityContext& target, BOOL &allowed);
	/**< Check if the target context fulfills static properties to be
	     considered a candidate for cross-origin access from the given
	     source context. Does not perform the steps required by CORS,
	     but will filter out attempted accesses that shouldn't be
	     considered by CORS, as they're bound to not be allowed, or
	     otherwise fail.

	     @param source The source/origin security context.
	     @param target The target context being checked for cross-origin
	            access.
	     @param [out]allowed TRUE if it should be considered a CORS
	            enabled access; FALSE if not.
	     @return OpStatus::OK on success; OpStatus::ERR_NO_MEMORY on OOM. */

	OP_STATUS CheckCrossOriginAccess(const OpSecurityContext& source, const OpSecurityContext& target, BOOL &allowed);
	/**< Given a target context that holds the response from an actual
	     CORS request, check if the resource allows access. This is
	     the final resource access check.

	     The source context must contain a OpCrossOrigin_Request.This is
	     a synchronous check, with the result being returned via 'allowed'.

	     @param source The source/origin security context.
	     @param target The target context being checked for cross-origin
	            access.
	     @param [out]allowed TRUE if it should be considered a CORS
	            enabled access; FALSE if not.
	     @return OpStatus::OK on success; OpStatus::ERR_NO_MEMORY on OOM. */

	static OP_STATUS CheckCrossOriginSecurity(const OpSecurityContext& source, const OpSecurityContext& target, OpSecurityCheckCallback* security_callback, OpSecurityCheckCancel **cancel);
	/**< Check that the source context can access a resource in the target
	     context, following CORS rules. Outcome is communicated via the
	     supplied callback. That is, if a successful outcome is reported
	     the caller is allowed to go ahead with the actual CORS enabled
		 request. The second phase, verifying the response to that actual
		 request, is handled by CheckCrossOriginAccess().

	     @param source The source/origin security context.
	     @param target The target context being checked for cross-origin
	            access.
	     @param security_callback The security callback.
	     @param [out]security_cancel If non-NULL, the security check's
	            cancellation method.
	     @return OpStatus::OK on successful completion of the check;
	             OpStatus::ERR_NO_MEMORY on OOM. */

	static OP_STATUS CheckCrossOriginSecurity(const OpSecurityContext& source, const OpSecurityContext& target, BOOL &allowed);
	/**< Check that the source context can access a resource in the target
	     context, following the protocol and constraints specified by
	     CORS. This is a synchronous check for a "simple CORS request."

	     @param source The source/origin security context.
	     @param target The target context being checked for cross-origin
	            access.
	     @param [out]allowed TRUE if it should be considered a CORS
	            enabled access; FALSE if not.
	     @return OpStatus::OK on success; OpStatus::ERR_NO_MEMORY on OOM. */

private:
	OpCrossOrigin_Manager *cross_origin_manager;
	/**< The CORS manager instance. We keep only one. */
};
#endif // CORS_SUPPORT
#endif // SECURITY_CROSS_ORIGIN_H
