/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

/*
   Cross-module security manager: DOM model
   Lars T Hansen.

   See documentation/document-security.txt in the documentation directory.  */
#ifndef SECURITY_DOM_H
#define SECURITY_DOM_H

class OpSecurityContext;
class OpSecurityState;
class OpSecurityCheckCallback;
class OpSecurityCheckCancel;
class DOM_Runtime;

class OpSecurityManager_DOM
{
public:
	OpSecurityManager_DOM();

#ifdef SECMAN_ALLOW_DISABLE_XHR_ORIGIN_CHECK
	OP_STATUS SetSkipXHROriginCheck(DOM_Runtime* runtime, BOOL& state);
	/**< Sets a flag in the runtime which causes it to skip origin checks
	     while doing XHR. This allows cross-domain XHR, use with caution.
		 This function will return OpStatus::FAILED unless AllowSetSkiptXHROriginCheck
		 has been set.*/

	void AllowSetSkipXHROriginCheck(BOOL& allow);
	/**< This will set a flag to allow the SetSkipXHROriginCheck to be run. */
#endif // SECMAN_ALLOW_DISABLE_XHR_ORIGIN_CHECK

	BOOL AllowedToNavigate(const OpSecurityContext& source, const OpSecurityContext& target);
	/**< Checks if a context is allowed to navigate another context, e.g., by changing
	     window.location. Based on policy in HTML5 spec. */

#ifndef SELFTEST
private:
#endif //SELFTEST
	static double GetProtocolMatchRank(const OpString& protocol, const OpString& pattern);
	/**< Returns the number telling how well the protocol name matches the given pattern;
	 * e.g. 1.0 means a perfect match. */

protected:

#ifdef SECMAN_ALLOW_DISABLE_XHR_ORIGIN_CHECK
	BOOL allow_set_skip_origin_check;
#endif // SECMAN_ALLOW_DISABLE_XHR_ORIGIN_CHECK

	OP_STATUS CheckLoadSaveSecurity(const OpSecurityContext& source, const OpSecurityContext& target, BOOL& allowed, OpSecurityState& state);
	/**< Check that the source context can run a specific script in the target
	     context. The target context must include the source code for the script. */

	OP_STATUS CheckLoadSaveSecurity(const OpSecurityContext& source, const OpSecurityContext& target, OpSecurityCheckCallback* security_callback, OpSecurityCheckCancel **security_cancel);
	/**< Check that the source context allows loading or saving resources
	     at the target context. Callback is notified of outcome. */

	BOOL CheckOperaConnectSecurity(const OpSecurityContext &source);
	/**< Check that the source context is allowed access to the 'opera.connect' API.
	     The source context must include the runtime making the request. */

#ifdef JS_SCOPE_CLIENT
	BOOL CheckScopeSecurity(const OpSecurityContext &source);
		/**< Check that the source context is allowed access to the 'opera.scope*'
		     API functions. The source context must include the runtime making
		     the request. */
#endif // JS_SCOPE_CLIENT

#ifdef EXTENSION_SUPPORT
	OP_STATUS CheckExtensionSecurity(const OpSecurityContext& source, const OpSecurityContext& target, BOOL& allowed);
	/**< Check that the source context can run a specific script in the target
	     context, optionally owned by an extension. The target context must
	     include the source URL and the extension owning it. */
#endif // EXTENSION_SUPPORT

#if defined(DOCUMENT_EDIT_SUPPORT) && defined(USE_OP_CLIPBOARD)
	BOOL CheckClipboardAccess(const OpSecurityContext& source);
	/**< Check whether the context is allowed to read and write into the
	     system clipboard. */
#endif // DOCUMENT_EDIT_SUPPORT && USE_OP_CLIPBOARD

#ifdef WEB_HANDLERS_SUPPORT
	OP_STATUS CheckWebHandlerSecurity(const OpSecurityContext& source, const OpSecurityContext& target, BOOL& allowed);
	/**< Check that a page is allowed to register handler for a protocol
	     stored in the target security context's text data. The page and
	     handler URLs are stored in the source and target contexts,
	     respectively. */
#endif// WEB_HANDLERS_SUPPORT

#ifdef DOM_WEBWORKERS_SUPPORT
	OP_STATUS CheckWorkerScriptImport(const OpSecurityContext& source, const OpSecurityContext& target, BOOL& allowed);
	/**< Check that a Web Worker's source context is allowed to import a
	     a script from target. */
#endif // DOM_WEBWORKERS_SUPPORT

};

#endif // SECURITY_DOM_H
