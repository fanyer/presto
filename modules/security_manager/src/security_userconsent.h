/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef SECURITY_USERCONSENT_H
#define SECURITY_USERCONSENT_H

#ifdef SECMAN_USERCONSENT

#include "modules/device_api/OpListenable.h"
#include "modules/prefs/prefsmanager/collections/pc_js.h"
#include "modules/security_manager/src/security_persistence.h"
#include "modules/windowcommander/OpWindowCommander.h"

class OpSecurityContext;
class OpSecurityCheckCallback;
class OpSecurityCheckCancel;
class UserConsentDialogCallback;
class UserConsentListener;
class UserConsentPersistenceProvider;


/** Listener to be notified of user consent change.
 */
class UserConsentListener
{
public:
	virtual ~UserConsentListener() {}

	/** Called when consent has been revoked.
	 */
	virtual void OnUserConsentRevoked() = 0;

	/** Called when the runtime is about to be destroyed.
	 *
	 * The listener should unregister itself if it hasn't already.
	 */
	virtual void OnBeforeRuntimeDestroyed(DOM_Runtime *runtime) = 0;
};


/**
 * OpSecurityManager_UserConsent is one of the base classes of OpSecurityManager.
 * It is responsible for security checking of operations performed by scripts
 * that require user's approval.
 *
 * The user's choice may be stored and used for subsequent operations of the
 * same kind depending on the persistence type set for the user's answer to
 * consent dialog.
 *
 * In private mode the persistence types other than runtime use a separate
 * storage which is cleared when private mode is exited.
 *
 * @author lasse@opera.com, msimonides@opera.com
 */
class OpSecurityManager_UserConsent
{
public:
	enum PermissionType {
#ifdef GEOLOCATION_SUPPORT
		PERMISSION_TYPE_GEOLOCATION,
#endif // GEOLOCATION_SUPPORT
#ifdef DOM_STREAM_API_SUPPORT
		PERMISSION_TYPE_CAMERA,
#endif // DOM_STREAM_API_SUPPORT
		PERMISSION_TYPE_MAX
	};

	/** Constructor */
	OpSecurityManager_UserConsent();

	/** Destructor */
	virtual ~OpSecurityManager_UserConsent();

#ifdef SECMAN_SET_USERCONSENT
	/** Get permissions stored for all documents in the window.
	 *
	 * This method provides information about saved permissions for sensitive
	 * APIs for the given window. The consent_information vector is filled with
	 * entries for each allowed or denied API for a given hostname. If there is
	 * no entry for a given API and host then user consent will be asked via
	 * OpPermissionListener if the document tries to use the API.
	 *
	 * A window may contain more than one document (e.g. when using iframes) so
	 * it is possible that there are more than one entries for a given API.
	 *
	 * @param window The window for which permissions are to be checked.
	 * @param consent_information Vector that is filled with entries for all
	 * permissions that have been allowed or denied.
	 *
	 * @return
	 *  - OK - success,
	 *  - ERR_NO_MEMORY - on OOM.
	 */
	OP_STATUS GetUserConsent(Window *window, OpVector<OpPermissionListener::ConsentInformation> &consent_information);

	/** Get permissions stored for all documents in the window.
	 *
	 * @param window The window for which permissions are to be checked.
	 * @param permission_type Which permission to check.
	 * @param consent_information Vector that is filled with entries for all
	 * permissions that have been allowed or denied.
	 *
	 * @return
	 *  - OK - success,
	 *  - ERR_NO_MEMORY - no memory.
	 */
	OP_STATUS GetUserConsent(Window *window, PermissionType permission_type, OpVector<OpPermissionListener::ConsentInformation> &consent_information);

	/** Set permission for a sensitive API.
	 *
	 * Set sensitive API permission for documents in a given window loaded
	 * from a given hostname. For PERSISTENCE_TYPE_SESSION and
	 * PERSISTENCE_TYPE_ALWAYS the setting may influence documents form
	 * the given host even if they are opened in other windows.
	 *
	 * Setting permission overrides permission set previously for different
	 * persistence (for the same operation and hostname).
	 *
	 * @param window The window for which permissions are being set.
	 * @param permission_type The operation/API for which permission is set.
	 * @param persistence_type Persistence for which permission is set.
	 *                         PERSISTENCE_TYPE_NONE is invalid.
	 * @param hostname Hostname for which permission is set.
	 * @param consent Whether permission is to be allowed or denied. ASK clears
	 *        the permission which means user will be asked for consent next
	 *        time the API is used.
	 * @return
	 *  - OK - success,
	 *  - ERR_OUT_OF_RANGE - permission_type or persistence_type is invalid,
	 *  - ERR_NO_MEMORY - on OOM.
	 */
	OP_STATUS SetUserConsent(Window *window, OpPermissionListener::PermissionCallback::PermissionType permission_type, OpPermissionListener::PermissionCallback::PersistenceType persistence_type, const uni_char *hostname, UserConsentType consent);
#endif // SECMAN_SET_USERCONSENT

	/** Register a user consent listener.
	 *
	 * The listener will be notified when permission permission_type is revoked
	 * for the given runtime.
	 * The listener is not owned by security manager and must be unregistered
	 * before it is destroyed.
	 *
	 * @param permission_type Permission for which the listener is registered.
	 * @param runtime Runtime for which the listener is registered.
	 * @param listener The listener to register.
	 *
	 * @return OK on success or ERR_NO_MEMORY on OOM.
	 */
	OP_STATUS RegisterUserConsentListener(PermissionType permission_type, DOM_Runtime *runtime, UserConsentListener *listener);

	/** Unregister a user consent listener.
	 *
	 * @param permission_type Permission for which the listener is unregistered.
	 * @param runtime Runtime for which the listener is unregistered.
	 * @param listener The listener to unregister.
	 */
	void UnregisterUserConsentListener(PermissionType permission_type, DOM_Runtime *runtime, UserConsentListener *listener);

	/** Clears all user permissions for a given type.
	 *
	 * All user consent listeners are notified of the change.
	 */
	void ClearUserConsentPermissions(PermissionType permission_type);

	/** Clear all private mode permissions.
	 *
	 * Private mode permissions are stored separately and cleared when
	 * private mode is closed. This way when private mode is opened again
	 * (possibly by a different person using the same computer) there are
	 * no permissions from previous session.
	 */
	void OnExitPrivateMode();

	/** Clean up runtime-related data.
	 *
	 * Notifies all consent listeners that the runtime is about to be destroyed
	 * and cleans up per-runtime data structures.
	 *
	 * Call before a runtime is about to be destroyed.
	 */
	void OnBeforeRuntimeDestroy(DOM_Runtime *runtime);

	/** Prepare for document being suspended.
	 *
	 * Closes all pending user consent dialogs and suspends them for when
	 * the document is resumed.
	 *
	 * In case of out-of-memory errors any pending dialogs will be closed
	 * and the operations denied.
	 */
	void OnDocumentSuspend(FramesDocument *frm_doc);

	/** Resume suspended user consent dialogs.
	 */
	void OnDocumentResume(FramesDocument *frm_doc);

	/** Prepare for document being destroyed.
	 *
	 * Destroys any suspended dialog callbacks for the given document
	 * (since it will not be resumed after all).
	 */
	void OnDocumentDestroy(FramesDocument *frm_doc);

#ifdef SELFTEST
	/** A host name for use in selftests.
	 *
	 * Selftests are run in "opera:blanker" which has no hostname. This
	 * function returns a host name to use in selftests.
	 */
	static const uni_char *GetSelftestHostName() { return UNI_L("opera-selftest"); }
#endif // SELFTEST

	/** Extract host name from the given URL.
	 *
	 * @return Pointer to the host name or NULL.
	 */
	// This function returns a dummy hostname in selftests (see
	// GetSelftestHostName), use it instead of extracting the hostname
	// directly from a URL.
	static const uni_char *GetHostName(const URL &url);
protected:
#ifdef GEOLOCATION_SUPPORT
	/** Checks geolocation security access permission.
	  *
	  * NOTE: The user will be asked for permission if needed.
	  *
	  * @param source Context that contains the url of the document
	  * that is trying to access certain resource (could be an iframe).
	  * @param callback will be called once the check is finished or an error occurs.
	  * @param cancel callback provided to callee to cancel this operation if needed.
	  * @return OK if successful, ERR for other errors.
	  */
	OP_STATUS CheckGeolocationSecurity(const OpSecurityContext& source, OpSecurityCheckCallback *callback, OpSecurityCheckCancel **cancel);
#endif // GEOLOCATION_SUPPORT
#ifdef DOM_STREAM_API_SUPPORT
	/** Checks camera access permission.
	  *
	  * NOTE: The user will be asked for permission if needed.
	  *
	  * @param source Context that contains the url of the document
	  * that is trying to access certain resource (could be an iframe).
	  * @param callback will be called once the check is finished or an error occurs.
	  * @param cancel callback provided to callee to cancel this operation if needed.
	  * @return OK if successful, ERR for other errors.
	  */
	OP_STATUS CheckCameraSecurity(const OpSecurityContext& source, OpSecurityCheckCallback *callback, OpSecurityCheckCancel **cancel);
#endif // DOM_STREAM_API_SUPPORT

private:
	class ClearRuntimeConsentForEachListener;
	class NotifyUserConsentListenersForEachListener;
	class RuntimeConsentEntry;
	friend class ClearRuntimeConsentForEachListener;
	friend class NotifyUserConsentListenersForEachListener;
	friend class RuntimeConsentEntry;
	friend class UserConsentDialogCallback;
	friend class UserConsentPersistenceProvider;

	OP_STATUS CheckUserConsentSecurity(const OpSecurityContext& source, OpSecurityPersistenceProvider* persistence_provider, PermissionType permission_type, OpSecurityCheckCallback *callback, OpSecurityCheckCancel **cancel);

	OP_STATUS AskForConfirmation(const OpSecurityContext& source,
								 OpSecurityPersistenceProvider *persistence_provider,
								 PermissionType permission_type,
								 OpSecurityCheckCallback* security_callback,
								 OpSecurityCheckCancel** cancel);

	/** Propagate answer to open consent dialogs.
	 *
	 * Used when multiple dialogs for the same permission have been opened in
	 * different windows, the user made a choice in one of them and her choice
	 * satisfies other open dialogs.
	 *
	 * Each dialog matching the permission_type, host and privacy mode is
	 * closed with the is_allowed answer.
	 *
	 * @param permission_type Permission of dialogs to process.
	 * @param host Host name of dialogs to process.
	 * @param privacy_mode Whether to process dialogs for documents in privacy mode.
	 * @param persistence_type Persistence type of the answer.
	 * @param is_allowed The answer to the dialogs.
	 */
	void ProcessPendingRequests(PermissionType permission_type, const char *host, BOOL privacy_mode, OpPermissionListener::PermissionCallback::PersistenceType persistence_type, BOOL is_allowed);

	void ClearSessionPermissions(PermissionType permission_type, BOOL privacy_mode);

	/** @short Set permission state.
	  *
	  * Note: if privacy_mode is TRUE then PERSISTENCE_SESSION and
	  * PERSISTENCE_FULL are stored for the duration of the privacy mode
	  * session, separately from regular permissions.
	  *
	  * @param permission_type Permission to set.
	  * @param hostname The host name to set permission state on
	  *                 (only for PERMISSION_SESSION and PERMISSION_FULL).
	  * @param runtime The DOM_Runtime this request originates from.
	  *                Must be non-NULL if type == PERMISSION_RUNTIME.
	  * @param type What persistency should this setting have.
	  * @param privacy_mode Whether the permission should be set for privacy mode.
	  * @param allow TRUE if allowed, FALSE if disallowed.

	  * @return OK if successful, OOM or ERR for other errors.
	  */
	OP_STATUS SetPermission(PermissionType permission_type, const uni_char *hostname, DOM_Runtime *runtime, ChoicePersistenceType type, BOOL privacy_mode, BOOL allow);

	/** @short Get permission state.
	 *
	 * Checks permission for a given operation, hostname, runtime and privacy
	 * mode.
	 * The function returns whether permission is allowed/denied/not set and
	 * persistence for which it is set.
	 *
	 * If the hostname is NULL, the returned permission is always DENIED.
	 * This is to block protocols like data:, javascript: and other that have
	 * not been considered.
	 *
	 * @param permission_type Permission to check.
	 * @param hostname The host name for which to check permisison state.
	 * @param runtime The DOM_Runtime to check the permission state for. May be NULL.
	 * @param privacy_mode Whether the permission is to be checked in privacy mode.
	 * @param persistence Set to the persistence for which the permission is set.
	 *                    The value is undefined when MAYBE is returned.
	 */
	UserConsentType GetPermission(PermissionType permission_type, const uni_char *hostname, DOM_Runtime *runtime, BOOL privacy_mode, ChoicePersistenceType *persistence) const;

	UserConsentType GetRuntimePermission(PermissionType permission_type, DOM_Runtime *runtime) const;
	OP_STATUS SetRuntimePermission(PermissionType permission_type, DOM_Runtime *runtime, UserConsentType consent);

	static OpPermissionListener::PermissionCallback::PermissionType ToPermissionListenerPermission(PermissionType permission_type);
	static PrefsCollectionJS::integerpref PrefsForPermissionType(PermissionType permission_type);

	void NotifyUserConsentListeners(PermissionType permission_type, DOM_Runtime *runtime);

	/** Close dialogs with "deny" response.
	 *
	 * For each dialog callback on the list cancels its dialog (if displayed)
	 * and sends a disallowed response to its security callbacks.
	 * Useful only for cleanup.
	 *
	 * @param dialog_callbacks List of dialog callbacks to process.
	 * @param runtime If not NULL, only dialog callbacks for the given runtime
	 *                will be closed.
	 */
	static void DisallowAllDialogs(const List<UserConsentDialogCallback> *dialog_callbacks, DOM_Runtime *runtime);

	class SessionConsentEntry
	{
	public:
		OpString hostname;
		BOOL is_allowed;
	};

	OP_STATUS AddSessionConsentEntry(OpStringHashTable<SessionConsentEntry> *session_permissions, const uni_char *hostname, BOOL is_allowed);

	OpStringHashTable<SessionConsentEntry> m_session_permissions[PERMISSION_TYPE_MAX];
	OpStringHashTable<SessionConsentEntry> m_privacy_mode_session_permissions[PERMISSION_TYPE_MAX];
	List<UserConsentDialogCallback> m_permission_callbacks[PERMISSION_TYPE_MAX];
	OpPointerHashTable<FramesDocument, List<UserConsentDialogCallback> > m_suspended_dialogs;

	class UserConsentListenable
		: public OpListenable<UserConsentListener>
	{
	public:
		DECLARE_LISTENABLE_EVENT0(UserConsentListener, UserConsentRevoked);
		DECLARE_LISTENABLE_EVENT1(UserConsentListener, BeforeRuntimeDestroyed, DOM_Runtime *);
	};

	class RuntimeConsentEntry
	{
	public:
		RuntimeConsentEntry()
		{
			for (int i = 0; i < PERMISSION_TYPE_MAX; ++i)
				consent[i] = ASK;
		}

		UserConsentType consent[PERMISSION_TYPE_MAX];
		UserConsentListenable listeners[PERMISSION_TYPE_MAX];
	};

	OP_STATUS CreateRuntimeConsentEntry(RuntimeConsentEntry *&consent, DOM_Runtime *runtime);

	OpPointerHashTable<DOM_Runtime, RuntimeConsentEntry> m_runtime_consent;

#ifdef SECMAN_SET_USERCONSENT
	/** Notify UserConsentListeners with a given hostname and permission type.
	 *
	 * Use with OpPointerHashTable<DOM_Runtime, RuntimeConsentEntry>::ForEach()
	 */
	class NotifyUserConsentListenersForEachListener : public OpHashTableForEachListener
	{
	public:
		NotifyUserConsentListenersForEachListener(const uni_char *hostname, PermissionType permission_type)
			: m_hostname(hostname), m_permission_type(permission_type) { }

		virtual void HandleKeyData(const void *key, void *data);
	private:
		const uni_char *m_hostname;
		PermissionType m_permission_type;
	};
#endif // SECMAN_SET_USERCONSENT

	/** Set all permissions of a given type to ASK.
	 *
	 * Use with OpPointerHashTable<DOM_Runtime, RuntimeConsentEntry>::ForEach()
	 */
	class ClearRuntimeConsentForEachListener : public OpHashTableForEachListener
	{
	public:
		ClearRuntimeConsentForEachListener(OpSecurityManager_UserConsent::PermissionType permission_type)
			: m_permission_type(permission_type) { }

		virtual void HandleKeyData(const void* key, void* data);
	private:
		PermissionType m_permission_type;
	};
};

#endif // SECMAN_USERCONSENT
#endif // SECURITY_USERCONSENT_H
