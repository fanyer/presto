/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef SECURITY_GADGET_H
#define SECURITY_GADGET_H

#ifdef GADGET_SUPPORT

#include "modules/security_manager/src/security_gadget_representation.h"

class OpGadget;
class OpGadgetClass;
class OpGadgetAccess;
class XMLFragment;
class CryptoCertificateStorage;

#ifdef SIGNED_GADGET_SUPPORT
class CryptoCertificate;
#endif // SIGNED_GADGET_SUPPORT

/**
 * OpSecurityContext_Gadget is a container for context information of the gadget
 * that is trying to grant permission for a certain operation or access to a
 * certain resource.
 *
*/

class OpSecurityContext_Gadget
{
public:

	/** Constructor */
	OpSecurityContext_Gadget() : gadget(NULL) {};

	/** Destructor */
	virtual ~OpSecurityContext_Gadget() {};

	OpGadget* GetGadget() const { return gadget; }
		/**< Return the gadget associated with this context, or NULL */

	BOOL IsGadget() const { return gadget != NULL; }
		/**< Return TRUE if this context is known to represent a gadget,
	         otherwise FALSE.  */

protected:
	OpGadget* gadget;
};

/**
 * OpSecurityManager_Gadget is one of the base classes of OpSecurityManager.
 * It is responsible for security checking of operations performed by gadgets.
 * Usually, these operations consist on resource access (protocol, host, port, path).
 *
 * This class contains an instance of current global security policy,
 * which can be loaded via widgets.xml file. If such file does not exist, or
 * in case of parsing error, the built-in default global policy is loaded.
 *
 * Main functionality if this class is checking if gadget A may have access
 * to resource X. Method CheckGadgetSecurity is used for this purposed, invoked
 * from its derived class (OpSecurityManager). CheckGadgetSecurity first checks
 * if operation is allowed by global policy. If allowed, it is then checked against
 * gadget policy, if exists.
 *
 * Other functionalities and members work together in order to parse policies.
 * Widget policies are parsed and checked against global widgets security policy.
 * Widget policies must equivalent or more restrictive than the global. Otherwise,
 * widget is not allowed to load.
 *
 * For further info, see specs:
 *	- modules/security_manager/documentation/widget-security.txt
 *	- https://ssl.opera.com:8008/developerwiki/Widgets/Security
 *
 * @author lasse@opera.com
*/

class OpSecurityManager_Gadget
{
friend class GadgetUrlFilter;
friend class PostSignatureVerificationGadgetSecurityCheck;

public:

	/** Constructor */
	OpSecurityManager_Gadget();

	/** Destructor */
	virtual ~OpSecurityManager_Gadget();

	void ConstructL();

	/** Sets global policy. Set the global gadget policy by parsing
	  * the XML fragment and generating an internal policy data structure.
	  * @param policy Pointer to XML fragment extracted from widgets.xml.
	  * @return OK if successful, ERR for other errors.
	  */
	static OP_STATUS SetGlobalGadgetPolicy(XMLFragment* policy);

#ifdef GADGETS_MUTABLE_POLICY_SUPPORT
	/** Override the global policy for a certain gadget instance.
	  *
	  * WARNING: This is normally not allowed, and should only be used
	  * for special testing purposes.
	  *
	  * @see OpGadget::SetAllowGlobalPolicy
	  *
	  * @param policy Pointer to XML fragment, as if it were extracted
	  *               from widgets.xml.
	  * @return OpStatus::OK if successful, OpStatus::ERR if policy
	  *         overriding is disallowed for this instance.
	  */
	static OP_STATUS SetGlobalGadgetPolicy(OpGadget* gadget, XMLFragment* policy);

	/** Override the global policy for a certain gadget instance.
	  *
	  * WARNING: This is normally not allowed, and should only be used
	  * for special testing purposes.
	  *
	  * @see OpGadget::SetAllowGlobalPolicy
	  * @param xml Full xml contents, as if read from widgets.xml.
	  * @return OpStatus::OK on success. OpStatus::ERR either means policy overriding
	  *         is disallowed for this instance, or 'xml' does not contain valid XML.
	  *         Otherwise, OpStatus::ERR_NO_MEMORY.
	  */
	static OP_STATUS SetGlobalGadgetPolicy(OpGadget* gadget, const uni_char* xml);

	/** Clear the global policy override for a certain gadget instance.
	  *
	  * @param gadget The gadget to clear the override for.
	  */
	static void ClearGlobalGadgetPolicy(OpGadget* gadget);
#endif // GADGETS_MUTABLE_POLICY_SUPPORT

	/** Sets the gadget policy for the gadget by parsing the XML fragment and
	  * the XML fragment and generating an internal policy data structure.
	  * It must not override global policy.
	  * @param gadget Pointer to gadget class.
	  * @param policy Pointer to XML fragment extracted from config.xml.
	  * @return OK if successful, ERR if policy tries to override global policy or other errors.
	  */
	static OP_STATUS SetGadgetPolicy(OpGadgetClass* gadget, XMLFragment* policy, OpString& error_reason);

	/** Sets the gadget policy for the gadget.
	  * It must not override global policy.
	  *
	  * NOTE: This is the W3C version (widget-access)
	  *
	  * @param gadget Pointer to gadget class.
	  * @param policy Pointer to OpGadgetAccess constructed from config.xml.
	  * @return OK if successful, ERR if policy tries to override global policy or other errors.
	  */
	static OP_STATUS AddGadgetPolicy(OpGadgetClass* gadget, OpGadgetAccess* policy, OpString& error_reason);

	/** Sets the default gadget policy for the gadget. This policy is built-in, i.e., it
	  * is not stored in an external xml file.
	  * @param gadget Pointer to gadget class.
	  * @return OK if successful, ERR for other errors.
	  */
	static OP_STATUS SetDefaultGadgetPolicy(OpGadgetClass *gadget);

	/** Constructs the default global policy. This policy is built-in, i.e., it
	  * is not stored in an external xml file.
	  * @param policy Pointer to newly initialized default policy
	  * @return OK if successful, ERR for other errors.
	  */
	static OP_STATUS SetDefaultGadgetPolicy(GadgetSecurityPolicy **policy);
	static OP_STATUS SetDefaultGadgetPolicy(); 	// For backward compatibility

#ifdef SIGNED_GADGET_SUPPORT
	/** Constructs the default global signed policy. This policy is built-in, i.e., it
	  * is not stored in an external xml file.
	  * @param policy Pointer to newly initialized default policy
	  * @return OK if successful, ERR for other errors.
	  */
	static OP_STATUS SetDefaultSignedGadgetPolicy(GadgetSecurityPolicy **policy);
#endif // SIGNED_GADGET_SUPPORT

	/** Constructs the default RESTRICTED global policy. This policy is built-in, i.e., it
	  * is not stored in an external xml file.
	  * This is used as a failsafe if parsing of the user supplied policy fails.
	  * @return OK if successful, ERR for other errors.
	  */
	static OP_STATUS SetRestrictedGadgetPolicy();

	/** Applies certain rules on the gadget policy which can only be decided upon after parsing in complete.
	  * @param gadget Pointer to gadget class.
	  */
	static OP_STATUS PostProcess(OpGadgetClass *gadget, OpString& error_reason);

	OP_STATUS ReloadGadgetSecurityMap();
	/**< Reload the security configuration document. */

#if defined(SIGNED_GADGET_SUPPORT) && defined(SECMAN_PRIVILEGED_GADGET_CERTIFICATE)
	/**
	 * Sets the privileged root CA certificate.
	 *
	 * This certificate is used to identify widgets with elevated privileges -
	 * signed widgets whose certification chain ends with this root CA cert are
	 * granted higher privileges.
	 *
	 * @param certificate The root CA certificate, this function takes ownership
	 * of the certificate object.
	 * @see OpGadgetClass::IsSignedWithPrivilegedCert
	 */
	void SetGadgetPrivilegedCA(CryptoCertificate* certificate);

	CryptoCertificate* GetGadgetPrivilegedCA() const { return privileged_certificate; }
#endif // SECMAN_PRIVILEGED_GADGET_CERTIFICATE && SIGNED_GADGET_SUPPORT

protected:
	friend class OpGadgetManager;
	friend class OpGadgetClass;
	friend class SuspendedSecurityCheck;

	GadgetSecurityPolicy* unsigned_policy;			///< Global gadget security policy. May not be NULL.
#ifdef SIGNED_GADGET_SUPPORT
	GadgetSecurityPolicy* signed_policy;			///< Global signed gadget security policy.
#endif // SIGNED_GADGET_SUPPORT
	Head gadget_security_policies;

	/** Checks security access permission for gadgets.
	  *
	  * Use the callback version below whenever possible.
	  *
	  * @param source Context that contains a pointer to gadget
	  * class that is trying to access certain resource.
	  * @param target Context that contains URL that gadgets is trying to access.
	  * @param allowed Result of security check (allowed or not allowed).
	  * @param state State of security check.
	  * @return OK if successful, ERR for other errors.
	  */
	OP_STATUS CheckGadgetSecurity(const OpSecurityContext& source, const OpSecurityContext& target, BOOL& allowed, OpSecurityState& state);

	/** Checks security access permission for gadgets.
	  * @param source Context that contains a pointer to gadget
	  * class that is trying to access certain resource.
	  * @param target Context that contains URL that gadgets is trying to access.
	  * @param callback Callback to use to report success/errors.
	  * @return OK if successful, ERR for other errors.
	  */
	OP_STATUS CheckGadgetSecurity(const OpSecurityContext& source, const OpSecurityContext& target, OpSecurityCheckCallback* callback);

	/** Checks security access permission between gadgets and jsplugin api:s
	  * @param source Context that contains a pointer to gadget
	  * class that is trying to access certain resource.
	  * @param allowed Result of security check (allowed or not allowed).
	  * @return OK if successful, ERR for other errors.
	  */
	OP_STATUS CheckGadgetJSPluginSecurity(const OpSecurityContext& source, const OpSecurityContext&, BOOL& allowed);

	/** Checks whether a certain URL should be allowed access to DOM APIs intended for gadgets.
	  * @param source Context that contains the source URL.
	  * @param target Context that contains the Gadget we want to access.
	  * @param allowed Result of security check (allowed or not allowed).
	  * @return OK if successful, ERR for other errors.
	  */
	OP_STATUS CheckGadgetDOMSecurity(const OpSecurityContext& source, const OpSecurityContext& target, BOOL& allowed);

#ifdef GADGETS_MUTABLE_POLICY_SUPPORT
	/** Checks whether a certain DOM_Runtime should be allowed to change gadget security policies.
	  * @param source Context that contains the acessing DOM_Runtime.
	  * @param target Context that contains the accessed OpGadget.
	  * @param allowed Result of security check (allowed or not allowed).
	  * @return OK if successful, ERR for other errors.
	  */
	OP_STATUS CheckGadgetMutatePolicySecurity(const OpSecurityContext& source, const OpSecurityContext& target, BOOL& allowed);
#endif // GADGETS_MUTABLE_POLICY_SUPPORT

	/** Checks security access permission for gadgets that wants to load a url or inline.
	  *
	  * Use the callback version below whenever possible.
	  *
	  * @param source Context that contains a pointer to the gadget which wants to load the url/inline.
	  * @param target Context that contains the URL that the gadget wants to load.
	  * @param allowed Result of security check (allowed or not allowed).
	  * @param state State of security check.
	  * @return OK if successful, ERR for other errors.
	  */
	OP_STATUS CheckGadgetUrlLoadSecurity(const OpSecurityContext& source, const OpSecurityContext& target, BOOL& allowed, OpSecurityState& state);

	/** Checks security access permission for gadgets that wants to load a url or inline.
	  * @param source Context that contains a pointer to the gadget which wants to load the url/inline.
	  * @param target Context that contains the URL that the gadget wants to load.
	  * @param callback Callback to use to report success/errors.
	  * @return OK if successful, ERR for other errors.
	  */
	OP_STATUS CheckGadgetUrlLoadSecurity(const OpSecurityContext& source, const OpSecurityContext& target, OpSecurityCheckCallback* callback);

	/** Checks security access permission for gadgets that wants to open a URL in a new window in the default browser.
	  * @param source Context that contains a pointer to the gadget which wants to open the URL.
	  * @param target Context that contains URL that the gadget wants to open.
	  * @param allowed Result of security check (allowed or not allowed).
	  * @return OK if successful, ERR for other errors.
	  */
	OP_STATUS CheckGadgetExternalUrlSecurity(const OpSecurityContext& source, const OpSecurityContext& target, BOOL& allowed);

	/** Checks security access for document that wants to access widgetManager functionality.
	  * widgetManager functionality gives the document access to all widgets content files(referenced by widget:// urls).
	  * @param source Context that contains a url of the document which wants access to widget manager.
	  * @param allowed Result of security check (allowed or not allowed).
	  * @return OK if successful, ERR for other errors.
	  */
	OP_STATUS CheckHasGadgetManagerAccess(const OpSecurityContext& source, BOOL& allowed);

	/** Checks if a document with a gadget associated to it can navigate to another url.
	  * It only checks if the url that's being navigated out of is a widget url, normal urls are
	  * already checked by DOM_ALLOWED_TO_NAVIGATE (or should be checked by it if it's not the case).
	  * @param source Context that contains a FramesDocument (in a window with an associated gadget) that wants to navigate to another url.
	  * @param target Context that contains the target URL.
	  * @param allowed Result of security check (allowed or not allowed).
	  * @return OK always.
	  */
	OP_STATUS GadgetAllowedToNavigate(const OpSecurityContext& source, const OpSecurityContext& target, BOOL& allowed);

	/** Parses gadget policy. Used by OpSecurityManager_Gadget::SetGlobalGadgetPolicy.
	  * @param pp The newly allocated GadgetSecurityPolicy object will be returned here.
	  * @return OK if successful, ERR for other errors.
	  */
	DEPRECATED(static OP_STATUS ParseGadgetPolicy(XMLFragment* policy, int allow, GadgetSecurityPolicy** pp, OpString& error_reason));

	/** Parses gadget policy. Used by OpSecurityManager_Gadget::SetGlobalGadgetPolicy.
	  * @param pp Pointer to GadgetSecurityPolicy object, allocated by caller.
	  * @return OK if successful, ERR for other errors.
	  */
	static OP_STATUS ParseGadgetPolicy(XMLFragment* policy, int allow, GadgetSecurityPolicy* pp, OpString& error_reason);

	/** Parses gadget policy element. Used by OpSecurityManager_Gadget::AddGadgetPolicy.
	  * @param policy XMLFragment to parse from
	  * @param element_type Which type of element this is
	  * @param pp Pointer to GadgetSecurityPolicy object, allocated by caller.
	  * @return OK if successful, ERR for other errors.
	  */
	static OP_STATUS ParseGadgetPolicyElement(OpGadgetAccess* policy, int element_type, GadgetSecurityPolicy* pp, OpString& error_reason);

	/** Parses port range: %d-%d.
	  * @param expr Pointer to expression to be parsed.
	  * @param low Pointer to integer value that contains lower port value after parsing.
	  * @param high Pointer to integer value that contains high port value after parsing.
	  * @return OK if successful, ERR for other errors.
	  */
	static OP_STATUS ParseActionUrlElement(XMLFragment* policy, GadgetActionUrl** action, GadgetSecurityDirective* directive); // TODO: error reporting.

	/** Checks if a gadget policy is equivalent or less restrictive than global policy.
	  * @param gadget_policy Pointer to gadget policy to be checked against global policy.
	  * @param global_policy Pointer to global policy.
	  * @return OK if successful, ERR for other errors.
	  */
	static OP_STATUS HelpConstructGadgetPolicy(Head *list, GadgetActionUrl::FieldType ft, GadgetActionUrl::DataType data, const uni_char* value);

#ifdef SIGNED_GADGET_SUPPORT
	OP_STATUS ReadAndParseGadgetSecurityMap();
#endif // SIGNED_GADGET_SUPPORT
	OP_STATUS ReadAndParseGadgetSecurity(const uni_char *basepath, const uni_char *src, GadgetSecurityPolicy **policy);
	const GadgetSecurityPolicy *GetPolicy(OpGadget *gadget);

#ifdef EXTENSION_SUPPORT
	/** Checks whether a certain document should be allowed access to an extension's resource.
	  * @param source Context that contains the accessing source document.
	  * @param target Context that contains the owner extension and the URL requested access to.
	  * @param allowed Result of security check (allowed or not allowed).
	  * @return OK if successful; no current error conditions.
	  */
	OP_STATUS CheckGadgetExtensionSecurity(const OpSecurityContext &source, const OpSecurityContext &target, BOOL &allowed);
#endif // EXTENSION_SUPPORT

private:
	enum
	{
		ALLOW_BLACKLIST = 1,		// Handle the 'blacklist' element
		ALLOW_ACCESS    = 2,		// Handle the 'access' element
		ALLOW_CONTENT   = 4,		// Handle the 'content' element
		ALLOW_PRIVATE_NETWORK  = 8,	// Handle the 'private-network' element
		ALLOW_W3C_ACCESS = 16		// handle the W3C version of the 'access' element (origin="http://opera.com:1234" subdomains="true" & origin="*")
	};

	class CurrentState
	{
	public:
		CurrentState() : allowed(TRUE), is_intranet(FALSE) {}

		BOOL allowed;
		BOOL is_intranet;
	};

	OP_STATUS HelpCheckGadgetSecurity(const GadgetSecurityPolicy *local, const GadgetSecurityPolicy *global, const URL &url, CurrentState &cur_state, OpSecurityState &sec_state);
	BOOL AllowGadgetFileUrlAccess(const URL& source_url, const URL& target_url);

#ifdef SIGNED_GADGET_SUPPORT
# ifdef SECMAN_PRIVILEGED_GADGET_CERTIFICATE
	CryptoCertificate* privileged_certificate;
# endif // SECMAN_PRIVILEGED_GADGET_CERTIFICATE
#endif // SIGNED_GADGET_SUPPORT

};

#endif // GADGET_SUPPORT
#endif // SECURITY_GADGET_H
