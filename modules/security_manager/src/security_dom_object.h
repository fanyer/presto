/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef SECURITY_DOM_OBJECT_H
#define SECURITY_DOM_OBJECT_H

#ifdef SECMAN_DOMOBJECT_ACCESS_RULES

#include "modules/security_manager/src/security_persistence.h"
#include "modules/util/OpHashTable.h"
#include "modules/windowcommander/OpWindowCommander.h"

class OpSecurityContext;
class OpSecurityState;
class OpSecurityCheckCallback;
class WindowCommander;

/**
 * Container for context information on access to certain properties of a
 * DOM object.
 */
class OpSecurityContext_DOMObject
{
public:
	OpSecurityContext_DOMObject()
		: operation_name(NULL), argument1(NULL), argument2(NULL)
	{
	}

	OpSecurityContext_DOMObject(const char* operation_name, const uni_char* argument1, const uni_char* argument2)
		: operation_name(operation_name), argument1(argument1), argument2(argument2)
	{
	}

	BOOL IsDOMObjectAccess() const		{ return operation_name != NULL; }

	const char* GetOperationName() const { return operation_name; }
	const uni_char* GetArgument1() const { return argument1; }
	const uni_char* GetArgument2() const { return argument2; }

private:
	const char* operation_name;
	const uni_char* argument1;
	const uni_char* argument2;
};


class OpGadget;

/** Security manager for protecting access to DOM object properties and functions.
 */
class OpSecurityManager_DOMObject
{
	friend class OpSecurityDialogCallback;
public:

	/** Permission types.
	 *
	 * They describe whether particular functionality is allowed and what kind of
	 * persistence is allowed (none, i.e. ask the user on each access/invocation,
	 * once per session or full persistency, i.e. user's choice may be stored
	 * and no further confirmation is required unless the setting is reset).
	 *
	 * Property access and function invocation are separated due to technical
	 * reasons (a function cannot be protected by property access as it may be
	 * assigned to a variable and called without accessing the original property).
	 */
	enum SecurityPermission
	{
		UNRESTRICTED = 0,				///< Always allowed.
		ALLOWED = 1,					///< Allowed. For gadgets, requires <feature> declaration in config.xml.
		ACCESS_ONE_SHOT = 2,			///< Requires user confirmation on each access and <feature> declaration.
		ACCESS_SESSION = 3,				///< Requires user confirmation once per session on access and <feature> declaration.
		ACCESS_PERSISTENT = 4,			///< User's choice is saved in persistent storage and <feature> declaration.
		INVOCATION_ONE_SHOT = 5,		///< Requires user confirmation on each invocation and <feature> declaration.
		INVOCATION_SESSION = 6,			///< Requires user confirmation once per session and <feature> declaration.
		INVOCATION_PERSISTENT = 7,		///< User's choice is saved in persistent storage and <feature> declaration.
		DISALLOWED = 8					///< Not allowed
	};

	/// The values of this enum are used to index array of rulesets,
	/// so they need to be consecutive numbers. Modify with caution.
	enum TrustDomain
	{
		TRUST_DOMAIN_FIRST				= 0,
		TRUST_DOMAIN_NO_WIDGET			= TRUST_DOMAIN_FIRST,
		TRUST_DOMAIN_WIDGET_UNSIGNED	= 1,
		TRUST_DOMAIN_WIDGET_SIGNED		= 2,
		TRUST_DOMAIN_WIDGET_PRIVILEGED	= 3,	// Signed with a privileged certificate
		TRUST_DOMAIN_LAST
	};

	/** Identifies a set of feature URIs.
	 *
	 * This is needed because different versions of JIL specify different
	 * method to feature URI mapping.
	 */
	enum FeatureSet
	{
		FEATURE_SET_FIRST		= 0,
		FEATURE_SET_REGULAR		= FEATURE_SET_FIRST,
		FEATURE_SET_JIL_1_0		= 1,
		FEATURE_SET_LAST
	};

	struct SecurityRule
	{
		SecurityPermission permission;
		OpPermissionListener::DOMPermissionCallback::OperationType operation_type;
	};

	OpSecurityManager_DOMObject();
	void ConstructL();

	virtual ~OpSecurityManager_DOMObject();

	enum AccessType {
		ACCESS_TYPE_FUNCTION_CALL,
		ACCESS_TYPE_PROPERTY_ACCESS
	};

	OP_STATUS CheckDOMObjectSecurity(const OpSecurityContext& source, const OpSecurityContext& target, OpSecurityCheckCallback* security_callback, AccessType access_type);

private:
	TrustDomain GetTrustDomain(const OpSecurityContext& security_context) const;
#ifdef GADGET_SUPPORT
	TrustDomain GetTrustDomain(OpGadget* gadget) const;
#endif // GADGET_SUPPORT

#ifdef GADGET_SUPPORT
	BOOL HasRequiredFeature(OpGadget* gadget, const char* operation_name) const;
#endif // GADGET_SUPPORT

	/** Creates a suitable persistence provider, based on the source and target context.
	 */
	OP_STATUS CreatePersistenceProvider(OpSecurityPersistenceProvider** new_provider, const OpSecurityContext& source_context, const OpSecurityContext& target_context, BOOL is_fully_persistent) const;

	/** Displays a dialog asking the user to allow the operation.
	 *
	 * \param permission_callback Callback that receives the answer. AskForConfirmation assumes ownership of the object.
	 * \param win_cmd WindowCommander used for displaying the dialog.
	 * \param operation_name name identifying the rule/operation.
	 * \param operation_type identifier of the operation (for dialog).
	 * \param persistence_provider object providing persistence of the user's choice. AskForConfirmation assumes ownership of the object.
	 * \param persistence the maximum allowed type of persisting the user's choice.
	 * \param argument1 first argument to the operation (may be NULL).
	 * \param argument2 second argument to the operation (may be NULL).
	 * \param url is the url of the associated OpSecurityContext that
	 *   asks for permission; the url can be displayed in a UI dialog.
	 */
	void AskForConfirmation(OpSecurityCheckCallback* permission_callback,
							WindowCommander* win_cmd,
							const char* operation_name,
							OpPermissionListener::DOMPermissionCallback::OperationType operation_type,
							OpSecurityPersistenceProvider* persistence_provider,
							ChoicePersistenceType persistence,
							const uni_char* argument1,
							const uni_char* argument2,
							const uni_char* url);

#ifdef SELFTEST
public:
#endif // SELFTEST
	OP_STATUS AddSecurityRule(TrustDomain trust_domain,
							  const char* operation_name,
							  SecurityPermission permission,
							  OpPermissionListener::DOMPermissionCallback::OperationType operation_type);

	OP_STATUS AddFeatureRule(FeatureSet feature_set,
							 const char* operation_name,
							 const char* feature_uri);
#ifdef SELFTEST
	OP_STATUS RemoveSecurityRule(TrustDomain trust_domain, const char* operation_name);
	OP_STATUS RemoveFeatureRule(FeatureSet feature_set, const char* operation_name);

private:
#endif // SELFTEST

	enum { RULESETS_COUNT = TRUST_DOMAIN_LAST - TRUST_DOMAIN_FIRST };
	OpAutoString8HashTable<SecurityRule> rulesets[RULESETS_COUNT];

	/** A class for mapping strings to other strings
	 *
	 * Neither keys nor values are managed by the object, they must exist
	 * as long as the map (e.g. static values).
	 */
	class OperationToFeatureMap
		: private OpString8HashTable<char> // Treated but not declared as const char because instatiation would fail on BREW.
	{
		friend OP_STATUS OpSecurityManager_DOMObject::AddFeatureRule(
				OpSecurityManager_DOMObject::FeatureSet feature_set,
				const char* operation_name,
				const char* feature_uri);
#ifdef SELFTEST
		friend OP_STATUS OpSecurityManager_DOMObject::RemoveFeatureRule(
				OpSecurityManager_DOMObject::FeatureSet feature_set, const char* operation_name);
#endif // SELFTEST

		OP_STATUS Add(const char* key, const char* data) { return OpString8HashTable<char>::Add(key, const_cast<char*>(data)); }
		OP_STATUS Remove(const char* key)
		{
			char* dummy;
			return OpString8HashTable<char>::Remove(key, &dummy);
		}
		void Delete(void* data) { OP_ASSERT(FALSE); }
	public:

		OP_STATUS GetData(const char* key, const char** data) const { return OpString8HashTable<char>::GetData(key, const_cast<char**>(data)); }
	};

	// TODO: a better data structure
	OperationToFeatureMap features[FEATURE_SET_LAST];

};

#endif // SECMAN_DOMOBJECT_ACCESS_RULES

#endif // SECURITY_DOM_OBJECT_H
