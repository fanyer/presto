/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

/** Cross-module security manager.

	\section init Initialization

	The security manager is started by SecurityManagerModule::InitL().
	Other startup code may register security policies with the manager.

	The manager is stopped by SecurityManagerModule::Destroy().

	Calls to CheckSecurity before initialization or after destruction will
	reliably return FALSE (ie, access disallowed).

	\section todo To do

	<ul>
	<li>   The manager is still very naive.
	<li>   Much logic must be moved from the DOM module and perhaps others.
	</ul>

	*/

#ifndef SECURITY_MANAGER_H
#define SECURITY_MANAGER_H

class DOM_Runtime;
class FramesDocument;
class URL;
class Comm;
class OpGadget;

#include "modules/security_manager/src/security_cross_origin.h"
#include "modules/security_manager/src/security_doc.h"
#include "modules/security_manager/src/security_docman.h"
#include "modules/security_manager/src/security_dom.h"
#include "modules/security_manager/src/security_dom_object.h"
#include "modules/security_manager/src/security_gadget.h"
#include "modules/security_manager/src/security_persistence.h"
#include "modules/security_manager/src/security_plugin.h"
#include "modules/security_manager/src/security_unite.h"
#include "modules/security_manager/src/security_userconsent.h"
#include "modules/security_manager/src/security_xslt.h"
#include "modules/security_manager/src/security_utilities.h"

/** An OpSecurityContext encapsulates a security context -- the origin or
	target of a request.  It is a convenience class that reduces the size
	of the protocol to the security manager, and therefore makes the
	security manager easier to use. */

class OpSecurityContext
	: public OpSecurityContext_DOC
	, public OpSecurityContext_Docman
#ifdef GADGET_SUPPORT
	, public OpSecurityContext_Gadget
#endif // GADGET_SUPPORT
	, public OpSecurityContext_Plugin
#ifdef SECMAN_DOMOBJECT_ACCESS_RULES
	, public OpSecurityContext_DOMObject
#endif // SECMAN_DOMOBJECT_ACCESS_RULES
#ifdef CORS_SUPPORT
	, public OpSecurityContext_CrossOrigin
#endif // CORS_SUPPORT
{
friend class OpSecurityManager;
public:
	OpSecurityContext() { Init(); }
		/**< Create an empty security context that carries no information. */

	OpSecurityContext(const URL &url);
		/**< Create a context based on a URL. */

	OpSecurityContext(const URL &url, const URL &ref_url);
		/**< Create a context based on a URL and Referrer URL. */

#ifdef GADGET_SUPPORT
	OpSecurityContext(OpGadget *gadget);
		/**< Create a context based on a gadget.  Gadget may not be NULL. */

	OpSecurityContext(OpGadget *gadget, Window *window);
		/**< Create a context based on a gadget and a window with said
		     gadget associated to it.  The gadget and the window may not be NULL. */
#endif // GADGET_SUPPORT

#if defined(DOCUMENT_EDIT_SUPPORT) && defined(USE_OP_CLIPBOARD)
	OpSecurityContext(const URL &url, Window *window);
		/**< Create a context based on a URL and a window. The window may be NULL. */
#endif // DOCUMENT_EDIT_SUPPORT && USE_OP_CLIPBOARD

	OpSecurityContext(DOM_Runtime* rt);
		/**< Create a context based on an DOM Runtime structure,
		     and whatever information it references.  The rt may not be
		     NULL, and it must not be deleted while this context is
		     alive. */

	OpSecurityContext(FramesDocument* frames_doc);
		/**< Create a context based on a FramesDocument, and whatever
		     information it references.  The document may not be NULL,
		     and it must not be deleted while this context is alive.  */

	OpSecurityContext(const URL &url, InlineResourceType inline_type, BOOL from_user_css);
		/**< Create a context based on a URL, an inline's type and information
		     whether the inline loading was initiated from a user CSS. */

	OpSecurityContext(const URL &url, DocumentManager* docman);
		/**< Create a context based on a URL and the DocumentManager where the
		     URL is opened as a document.  The document manager may not be NULL,
		     and it must not be deleted while this context is alive.  */

#ifdef EXTENSION_SUPPORT
	OpSecurityContext(const URL &url, OpGadget* extension);
		/**< Create a context based on a URL and extension. representing an attempt
		     to use the extension's resources at that URL. If the extension is NULL,
		     it represent access to a URL having no controlling owner. */
#endif // EXTENSION_SUPPORT

	OpSecurityContext(const URL &url, const char *charset, BOOL inline_element = FALSE);
		/**< Create a context based on a URL and a charset. The charset
		     is either that of the document, if loaded, or the one that
		     is to be set on the document. The pointer is owned by the
		     caller. Charset may not be NULL.
		     The inline_element being TRUE means charset is to be set on
		     e.g. a script or a css element. If FALSE the check applies to
		     documents (including iframes). */

#ifdef SECMAN_DOMOBJECT_ACCESS_RULES
	OpSecurityContext(const char* dom_access_rule, const uni_char* argument1 = NULL, const uni_char* argument2 = NULL);
		/**< Create a context based on DOM access rule and arguments.
		     \param dom_access_rule name/identifier of the rule for accessing the
		            dom property or calling the function.
		     \param argument1 optional argument to the operation, to be
		            presented to the user in security dialog. May be NULL.
		     \param argument2 optional argument to the operation, to be
		            presented to the user in security dialog. May be NULL. */
#endif // SECMAN_DOMOBJECT_ACCESS_RULES

#ifdef CORS_SUPPORT
	OpSecurityContext(DOM_Runtime* runtime, OpCrossOrigin_Request *cross_origin_request);
		/**< Create a context based on an DOM Runtime structure,
		     and whatever information it references.  The runtime may not be
		     NULL, and it must not be deleted while this context is
		     alive. If cross-origin access, obeying CORS, from the runtime's
		     origin is to be permitted, a value describing the cross-origin
		     request is also supplied. */

	OpSecurityContext(const URL &url, OpCrossOrigin_Request *cross_origin_request);
		/**< Create a context based on a URL and a cross-origin request.
		     The URL must not be deleted while this context is alive. The cross-origin
		     request may be NULL, meaning no cross-origin access is allowed. It must not
		     either be deleted while this context is alive. */
#endif // CORS_SUPPORT

	virtual ~OpSecurityContext();
		/**< Destroy the context.  */

	const URL& GetURL() const;
		/**< Return the URL associated with this context. */

	const URL& GetRefURL() const;
		/**< Return the Referrer URL associated with this context. */

	DOM_Runtime* GetRuntime() const;
		/**< Return the runtime associated with this context, or NULL. */

	void AddText(const uni_char* info);
		/**< Add information about the context in the form of a text string.
		     The information is interpreted relative to the security operation
		     in the call to OpSecurityManager::CheckSecurity().  The string
		     must be null-terminated and must not be deleted while this
		     OpSecurityContext element is alive.  */

	const uni_char* GetText() const { return text_info; }
		/**< Retrieve the information added with AddText. */

	Window* GetWindow() const { return window; }

	FramesDocument* GetFramesDocument() const { return doc; }
		/**< Return the FramesDocument associated with this context, or NULL. */

private:
	void Init();

private:
	URL url;
	URL ref_url;
	const uni_char *text_info;
	DOM_Runtime *origining_runtime;
	URL runtime_origin_url;
	/**< If no URL supplied or can otherwise be determined,
	     the origin URL of the runtime is kept as a copy. */
	Window *window;
};

/** A callback for notifying of security check completion.
 *
 * One and only one of the methods must be called. Once.
 */
class OpSecurityCheckCallback
{
public:
	virtual ~OpSecurityCheckCallback() {}

	/** Called when the security check is finished successfully.
	 *
	 * @param allowed Whether the operation is allowed or not.
	 * @param type What type of persistence this result was stored with (if any).
	 */
	virtual void OnSecurityCheckSuccess(BOOL allowed, ChoicePersistenceType type = PERSISTENCE_NONE) = 0;

	/** Called if the security check cannot be performed.
	 *
	 * @param error Code of error that caused the check not to be performed.
	 */
	virtual void OnSecurityCheckError(OP_STATUS error) = 0;
};

/** A callback to cancel an ongoing security check.
 */
class OpSecurityCheckCancel
{
public:
	/** Call this if you want to cancel the security check.
	 *
	 * NOTE: MUST NOT be called if any of the OpSecurityCheckCallback functions
	 * has been called. This object is then to be considered invalid.
	 */
	virtual void CancelSecurityCheck() = 0;
};

/** An OpSecurityState encapsulates state for multi-stage check operations
	(typically those that need to access the network, eg for host name lookups).  */

class OpSecurityState
{
public:
	OpSecurityState();
	~OpSecurityState();

	BOOL suspended;
		/**< Set to TRUE if the security check operation was suspended. */

	Comm *host_resolving_comm;
		/**< Not NULL iff we are waiting for host resolution. */
};

/** The OpSecurityManager class exports numerous static functions for
	setting policy and checking security, and constants used in making
	security checks.  */

class OpSecurityManager
	: public OpSecurityManager_DOM
	, public OpSecurityManager_DOC
	, public OpSecurityManager_Docman
#ifdef GADGET_SUPPORT
	, public OpSecurityManager_Gadget
#endif // GADGET_SUPPORT
	, public OpSecurityManager_Plugin
#ifdef WEBSERVER_SUPPORT
	, public OpSecurityManager_Unite
#endif // WEBSERVER_SUPPORT
#ifdef XSLT_SUPPORT
	, public OpSecurityManager_XSLT
#endif // XSLT_SUPPORT
	, public OpSecurityUtilities
#ifdef SECMAN_DOMOBJECT_ACCESS_RULES
	, public OpSecurityManager_DOMObject
#endif // SECMAN_DOMOBJECT_ACCESS_RULES
#ifdef SECMAN_USERCONSENT
	, public OpSecurityManager_UserConsent
#endif // SECMAN_USERCONSENT
#ifdef CORS_SUPPORT
	, public OpSecurityManager_CrossOrigin
#endif // CORS_SUPPORT
{
friend class SecurityManagerModule;
friend class OpSecurityManager_DOM;
friend class OpSecurityManager_DOC;
friend class OpSecurityManager_Docman;
#ifdef GADGET_SUPPORT
friend class OpSecurityManager_Gadget;
#endif // GADGET_SUPPORT
friend class OpSecurityManager_Plugin;
#ifdef WEBSERVER_SUPPORT
friend class OpSecurityManager_Unite;
#endif // WEBSERVER_SUPPORT
#ifdef XSLT_SUPPORT
friend class OpSecurityManager_XSLT;
#endif // XSLT_SUPPORT
friend class OpSecurityUtilities;
friend class DOM_Object;  // will be removed
#ifdef SECMAN_USERCONSENT
friend class OpSecurityManager_UserConsent;
#endif // SECMAN_USERCONSENT

public:
	/** The Operation enum defines all abstract operations that are subject to a security check.
		Each operation must be documented fully and the contents of the security contexts
		must be described.  The default security contexts contain only the protocol,
	    domain, and port of the source and target. */
	enum Operation
	{
		DOM_STANDARD,
			/**< Basic origin check. If CORS support is enabled, the
			     security check will check if the access to the target
			     context is allowed per CORS. */

		DOM_LOADSAVE,
			/**< A script wants to use DOM Load/Save. If CORS support is enabled,
			     the security check will determine if an attempted cross-origin
			     access is allowed to go ahead with the actual request. */

		DOM_ALLOWED_TO_NAVIGATE,
			/**< A script wants to navigate a window in another context. */

		DOM_OPERA_CONNECT,
			/**< A script wants to connect Opera to a remote debugger. If this
			     is allowed, access is granted to the opera.connect, opera.disconnect,
			     and opera.isConnected functions. */

#ifdef JS_SCOPE_CLIENT
		DOM_SCOPE,
			/**< A script wants to access Scope functionality via DOM. If this
			     is allowed, access is granted to opera.scopeEnableService,
			     opera.scopeAddClient, opera.scopeTransmit, and (if the
			     preprocessor wills it) opera.scopeSetTranscodingFormat. */
#endif // JS_SCOPE_CLIENT

		PLUGIN_RUNSCRIPT,
			/**< A plugin wants to execute a script.  The source context contains the origin
			     of the plugin; the target context includes the origin of the document
			     in which the plugin is embedded as well as the source code of the script
			     the plugin wants to run.  If the source code originates in a javascript: URL
			     (this is normally how it is) then that URL must have been fully decoded before
			     the script source is encoded. */

		DOC_SET_PREFERRED_CHARSET,
			/**< A document wants to set a preferred encoding on another resource,
			     such as a linked document or script, or a contained frame or iframe.
			     The source context includes the origin and charset of the parent document,
			     and the target context contains the URL of the resource on which the
			     charset is set, and the charset that you want to set. */

		DOC_INLINE_LOAD,
			/**< A document wants to load some inline element.
			     The source context includes the document which wants to load the inline,
			     and the target context contains the inline's URL, its type and a flag indicating
			     whether loading is initiated from a user CSS */

		DOCMAN_URL_LOAD,
			/**< A document manager wants to load a url.
			     The source context includes the source url and document manager and the target
			     context contains the url to be loaded. */

#ifdef XSLT_SUPPORT
		XSLT_IMPORT_OR_INCLUDE,
			/**< An XSL stylesheet imports or includes another stylesheet.  The other
			     stylesheet will not be used unless it is a well-formed XML document and a
			     valid XSL stylesheet. */

		XSLT_DOCUMENT,
			/**< An XSL transformation calls the XSLT document() function to load an XML
			     resource.  All information in the resource will be available to the page if
			     the resource is a well-formed XML document.  Very much like DOM_LOADSAVE. */
#endif // XSLT_SUPPORT

		GADGET_STANDARD,
			/**< Basic gadget url access check. */

		GADGET_EXTERNAL_URL,
			/**< A widget wants to open a URL in a new window in the default browser. */

		GADGET_ALLOWED_TO_NAVIGATE,
			/**< A document with a gadget associated to it wants to navigate to another url. */

		GADGET_JSPLUGINS,
			/**< A widget wants to use JS plugins. */

		GADGET_DOM,
			/**< A script wants to use DOM APIs intended for gadgets, e.g. 'window.widget'. */

#ifdef GADGETS_MUTABLE_POLICY_SUPPORT
		GADGET_DOM_MUTATE_POLICY,
			/**< A script wants change the policies of a gadget instance. */
#endif // GADGETS_MUTABLE_POLICY_SUPPORT

		UNITE_STANDARD,
			/**< Basic unite origin check. */

#ifdef SECMAN_DOMOBJECT_ACCESS_RULES
		DOM_ACCESS_PROPERTY,
			/**< A check for accessing (either put or get) a DOM property */

		DOM_INVOKE_FUNCTION,
			/**< A check for invoking a function */
#endif // SECMAN_DOMOBJECT_ACCESS_RULES

#ifdef GEOLOCATION_SUPPORT
		GEOLOCATION,
			/**< A script wants to use geolocation.
			     The first context must be built from a runtime requesting access,
			     the other context is ignored. */
#endif // GEOLOCATION_SUPPORT

#ifdef DOM_STREAM_API_SUPPORT
		GET_USER_MEDIA,
			/**< A script wants to use getUserMedia.
			     The first context must be built from a runtime requesting access,
			     the other context is ignored. */
#endif // DOM_STREAM_API_SUPPORT

#ifdef EXTENSION_SUPPORT
		DOM_EXTENSION_ALLOW,
			/**< Check if UserJS of an extension can activate. */

		DOM_EXTENSION_SCREENSHOT,
			/**< Check if an extension can take a screenshot of a viewport of a window.
			     source must contain a OpGadget object of the extension trying to take a screenshot
				 and target must contain a document screenshot of which has been requested.
			*/
#endif // EXTENSION_SUPPORT

#if defined(DOCUMENT_EDIT_SUPPORT) && defined(USE_OP_CLIPBOARD)
		DOM_CLIPBOARD_ACCESS,
			/**< A script wants to read or write clipboard content. Source
			     context contains a URL of the document trying to access the
			     clipboard and optionally a Window in whose context the check
			     is performed. The origin retrieved from the URL is checked
			     against site specific overrides. Window type is checked for
			     specific window types that can override origin check.
			     Target context is unused. */
#endif // DOCUMENT_EDIT_SUPPORT && USE_OP_CLIPBOARD

		GADGET_MANAGER_ACCESS,
			/**< A check for full access to all installed gadget resources and widgetmanager APIs.
			     Should only be granted to opera internal pages like opera:widgets. */

#ifdef WEB_HANDLERS_SUPPORT
		WEB_HANDLER_REGISTRATION,
			/**< A page wants to use register a content/protocol handler. */
#endif // WEB_HANDLERS_SUPPORT

#ifdef CORS_SUPPORT
		CORS_PRE_ACCESS_CHECK,
			/**< Check if target access is a cross-origin candidate.
			     Not a requirement to use by contexts wanting cross-origin
			     access, but filters out accesses that are known to be
			     within same origin or guaranteed to fail if considered
			     for a full cross-origin security check. A "stage 0"
			     security check. */

		CORS_RESOURCE_ACCESS,
			/**< Perform a cross-origin resource sharing check.
			     This verifies that the target resource does allow
			     cross-origin use. This is the final step of a
			     CORS-enabled security check. */
#endif // CORS_SUPPORT

#ifdef DOM_WEBWORKERS_SUPPORT
		WEB_WORKER_SCRIPT_IMPORT,
			/**< A Web Worker wants to import an external script. */
#endif // DOM_WEBWORKERS_SUPPORT

			LAST_OPERATION
			/**< Don't use. */
	};

	static OP_STATUS CheckSecurity(Operation op, const OpSecurityContext& source, const OpSecurityContext& target, BOOL& allowed);
		/**< Test if one context can access another with a given operation.

			 \param op       the operation
			 \param source   the source (accessing) context
			 \param target   the target (accessed) context
			 \param allowed  will be set to TRUE or FALSE as appropriate
			 \return         OpStatus:OK normally, OpStatus::ERR_NO_MEMORY on OOM.  */

	static OP_STATUS CheckSecurity(Operation op, const OpSecurityContext& source, const OpSecurityContext& target, BOOL& allowed, OpSecurityState& state);
		/**< Test if one context can access another with a given operation, possibly
		     in multiple stages

			 \param op       the operation
			 \param source   the source (accessing) context
			 \param target   the target (accessed) context
			 \param allowed  will be set to TRUE if the operation is allowed; FALSE if not or if not yet known
			 \param state    will be updated to hold state for multi-stage operations
			 \return         OpStatus:OK normally, OpStatus::ERR_NO_MEMORY on OOM.  */

	static OP_STATUS CheckSecurity(Operation op, const OpSecurityContext& source, const OpSecurityContext& target, OpSecurityCheckCallback* callback);
		/**< Test if one context can access another with a given operation.
		     The operation may be performed synchronously (by calling the
			 callback functions before returning) or asynchronously.

			 The callback is owned by the client code and cannot be deleted
			 until it is called or the method returns error.

			 Errors may be reported both by returning an appropriate value
			 and by calling the callback's OnSecurityCheckError method.
			 It is however preferred to use the callback only for reporting
			 asynchronous errors.

			 \param op			the operation
			 \param source		the source (accessing) context
			 \param target		the target (accessed) context
			 \param callback	will be called once the check is finished or
								an error occurs.
			 \return			\see OpStatus */

	static OP_STATUS CheckSecurity(Operation op, const OpSecurityContext& source, const OpSecurityContext& target, OpSecurityCheckCallback* callback, OpSecurityCheckCancel **cancel);
		/**< Test if one context can access another with a given operation.
		     The operation may be performed synchronously (by calling the
			 callback functions before returning) or asynchronously.

			 The callback is owned by the client code and cannot be deleted
			 until it is called or the method returns error.

			 Errors may be reported both by returning an appropriate value
			 and by calling the callback's OnSecurityCheckError method.
			 It is however preferred to use the callback only for reporting
			 asynchronous errors.

			 \param op			the operation
			 \param source		the source (accessing) context
			 \param target		the target (accessed) context
			 \param callback	will be called once the check is finished or
								an error occurs.
			 \param cancel		(out) can be used to cancel any pending security check.
								it is set to NULL if the check is synchronous and/or
								cannot be cancelled.
			 \return			\see OpStatus */
public:
	static BOOL OriginCheck(const OpSecurityContext& source, const OpSecurityContext& target);
		/**< Test whether the source context can access the target context according to the
			 same-origin policy: the two contexts must have the same protocol, host, and port.

			 Normally you should not use this function, use CheckSecurity() instead.

			 Additional restrictions may be imposed for some protocols.  For example, for the
			 file: protocol, content loaded from a file may not access content loaded from
			 a directory (ie generated HTML from a directory listing).

			 Hosts are compared textually: foo.bar.com is not the same as 128.223.1.47, even
			 if the former resolves as the latter.  A missing host name is meant to be taken
			 as "localhost".

			 Missing port numbers are resolved as the default port for some protocols, so
			 http://foo.bar.com:80 and http://foo.bar.com are the same.  (Currently this
			 fixup is done only for HTTP.)
			 */

	static void ResolveDefaultPort(URLType t, unsigned int& p);
		/**< If p is 0 then set it to the default port for the given protocol type,
			 for protocols where this is known and makes sense.  Otherwise leave it
			 as 0.  */

	static void GetDomainFromURL(const URL &url, const uni_char **domain, URLType *type, int *port, const ServerName **sn);
		/**< Extract protocol, domain, and port from a URL */

private:
	OpSecurityManager();
	virtual ~OpSecurityManager();

	void ConstructL();

	static BOOL OriginCheck(const URL& url1, const URL& url2);
		/**< Test whether the domains of the two URLs are the same.  Does not take
			 into account peculiarities of the protocols.  */

	static BOOL OriginCheck(URLType type1, unsigned int port1, const uni_char *domain1, URLType type2, unsigned int port2, const uni_char *domain2);
		/**< Test whether the two broken-down domains are the same.   Does not take
			 into account peculiarities of the protocols.  */

	static BOOL OriginCheck(URLType type1, unsigned int port1, const uni_char *domain1, BOOL domain1_overridden, int nettype1, URLType type2, unsigned int port2, const uni_char *domain2, BOOL domain2_overridden, int nettype2);
		/**< Test whether the two broken-down domains are the same.   Does not take
			 into account peculiarities of the protocols.  */

	static void ResolveLocalDomain(const uni_char*& domain);
		/**< If domain is NULL or "localhost", set it to "" */

	static BOOL LocalOriginCheck(FramesDocument *document1, FramesDocument *document2);
		/**< @return TRUE if the two documents should be considered to have the same domains,
			 taking document.domain into account.  */

	static FramesDocument* GetOwnerDocument(FramesDocument *document);
		/**< @return the parent document if defined, otherwise the opener if it is reliable,
					 otherwise NULL */

	static BOOL OperaUrlCheck(const URL& source);
		/**< Test wether the source comes from a trusted opera: page. */

#ifdef SELFTEST

public:

	/** Class of objects to be put on the stack in blocks of trusted code that can override security checks.
		Currently only enabled for SELFTEST in order to ease the write of selftests. Adding an object of this
		class on the stack will put it onto a global stack of priviliged blocks. That means you can have
		privileged blocks inside other privileged blocks. By default, it will override all security checks.
		You can make more fine-grained overrides by subclassing and override AllowOperation. */
	class PrivilegedBlock
		: public Link
	{
	public:
		PrivilegedBlock();
		virtual ~PrivilegedBlock();

		virtual BOOL AllowOperation(Operation op);
	};

private:
	/** VS6 compiler hack. */
	friend class PrivilegedBlock;

	/** Privileged block stack */
	Head m_privileged_blocks;

#endif // SELFTEST
};

class OpSecurityCallbackOwner
{
public:
	virtual void SecurityCheckCompleted(BOOL allowed, OP_STATUS error) = 0;
};

/** OpSecurityCallbackHelper. Hides the details of waiting on a
    security callback to complete. Notification of outcome is
    reported through the OpCrossOrigin_SecurityCallbackOwner interface. */
class OpSecurityCallbackHelper
	: public OpSecurityCheckCallback
{
public:
	OpSecurityCallbackHelper(OpSecurityCallbackOwner *owner)
		: error(OpStatus::OK)
		, is_finished(FALSE)
		, is_allowed(FALSE)
		, owner(owner)
	{
	}

	virtual ~OpSecurityCallbackHelper();

	/* From OpSecurityCheckCallback: */
	virtual void OnSecurityCheckSuccess(BOOL allowed, ChoicePersistenceType type = PERSISTENCE_NONE);
	virtual void OnSecurityCheckError(OP_STATUS status);

	BOOL IsFinished() { return is_finished; }
	BOOL IsAllowed() { return is_allowed; }

	void Cancel();
	/**< Drop owner interest in the callback object. The owner
	     must not use the object after this. The security
	     manager might, but the outcome of the security check won't
	     be reported back to the owner upon completion. */

	void Reset();
	/**< A finished callback object may be reused by the same
	     owner; a call to Reset() is required before doing so. */

	void Release();
	/**< Called by the owner to release the helper callback object.
	     It must not use (nor delete) the object after having done so. */

private:
	void Finish();

	OP_STATUS error;
	BOOL is_finished;
	BOOL is_allowed;
	OpSecurityCallbackOwner *owner;
};
#endif // !SECURITY_MANAGER_H
