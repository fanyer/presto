/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef SECMAN_USERCONSENT

#include "modules/doc/frm_doc.h"
#include "modules/dochand/winman_constants.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/ecmascript_utils/esobjman.h"
#include "modules/prefs/prefsmanager/collections/pc_js.h"
#include "modules/security_manager/include/security_manager.h"
#include "modules/security_manager/src/security_userconsent.h"
#include "modules/windowcommander/src/WindowCommander.h"
#include "modules/dom/domutils.h"

class UserConsentPersistenceProvider
	: public OpSecurityPersistenceProvider
{
public:
	static OP_STATUS Create(OpSecurityPersistenceProvider **new_obj, OpSecurityManager_UserConsent::PermissionType permission_type, const OpSecurityContext &source)
	{
		if (source.GetRuntime()
#ifdef GADGET_SUPPORT
			&& !source.GetGadget()
#endif // GADGET_SUPPORT
		)
		{
			BOOL privacy_mode = FALSE;
			if (source.GetWindow())
				privacy_mode = source.GetWindow()->GetWindowCommander()->GetPrivacyMode();

			*new_obj = OP_NEW(UserConsentPersistenceProvider, (permission_type, source.GetRuntime(), source.GetURL(), privacy_mode));
			return (*new_obj) ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
		}
		return OpStatus::ERR_NOT_SUPPORTED;
	}

	virtual ~UserConsentPersistenceProvider() {}

	// from OpSecurityPersistenceProvider
	UserConsentType IsAllowed(ChoicePersistenceType* persistence) const
	{
		return g_secman_instance->GetPermission(permission_type, OpSecurityManager_UserConsent::GetHostName(url), runtime, privacy_mode, persistence);
	}

	OP_STATUS SetIsAllowed(BOOL is_allowed, ChoicePersistenceType persistence)
	{
		return g_secman_instance->SetPermission(permission_type, OpSecurityManager_UserConsent::GetHostName(url), runtime, persistence, privacy_mode, is_allowed);
	}

private:
	UserConsentPersistenceProvider(OpSecurityManager_UserConsent::PermissionType permission_type, DOM_Runtime *runtime, const URL& url, BOOL privacy_mode)
		: runtime(runtime), url(url), permission_type(permission_type), privacy_mode(privacy_mode)
	{
	}

	DOM_Runtime *runtime;
	URL url;
	OpSecurityManager_UserConsent::PermissionType permission_type;
	BOOL privacy_mode;
};

class UserConsentDialogCallback;

/** Represents a request for user consent.
 *
 * Each request represents one call to CheckUserConsentSecurity that needs
 * requires asking the user for consent. Requests are associated with
 * UserConsentDialogCallback.
 */
class UserConsentRequest
	: public OpSecurityCheckCancel
	, public ListElement<UserConsentRequest>
{
public:

	/** Constructor.
	 *
	 * @param security_check_callback The callback to call with the answer.
	 * @param dialog_callback The dialog callback to attach to. It will take
	 *        ownership over this request object.
	 */
	UserConsentRequest(OpSecurityCheckCallback *security_check_callback, UserConsentDialogCallback *dialog_callback);
	~UserConsentRequest()
	{
		Out();
	}

	void OnSecurityCheckSuccess(BOOL allowed, ChoicePersistenceType persistence)
	{
		security_check_callback->OnSecurityCheckSuccess(allowed, persistence);
	}

	// From OpSecurityCheckCancel
	virtual void CancelSecurityCheck();

private:
	OpSecurityCheckCallback *security_check_callback;
	UserConsentDialogCallback *dialog_callback;
};


/** A callback for user consent dialog.
 *
 * Each callback is associated with at least one UserConsentRequest.
 * When it's called, it passes the user's decision to all associated
 * requests.
 *
 * The callback object is responsible for deleting itself when it
 * gets response from UI (OnAllowed, OnDisallowed) or is cancelled
 * (OnRequestCancelled).
 */
class UserConsentDialogCallback
	: public ListElement<UserConsentDialogCallback>
	, public OpPermissionListener::PermissionCallback
{
public:
	UserConsentDialogCallback(WindowCommander *wnd_cmd, OpSecurityPersistenceProvider *persistence_provider, OpSecurityManager_UserConsent::PermissionType type)
		: wnd_cmd(wnd_cmd)
		, persistence_provider(persistence_provider)
		, runtime(NULL)
		, type(type)
		, has_dialog(FALSE)
		, is_within_ask(FALSE)
		, is_finished_synchronously(FALSE)
	{
		OP_ASSERT(persistence_provider);
	}

	virtual ~UserConsentDialogCallback()
	{
		Out();
		OP_ASSERT(m_requests.Empty());
		OP_DELETE(persistence_provider);
	}

	OP_STATUS Construct(const URL &url, DOM_Runtime *runtime)
	{
		if (url.IsValid())
		{
			RETURN_IF_ERROR(url.GetAttribute(URL::KUniName, this->url));
			RETURN_IF_ERROR(url.GetAttribute(URL::KHostName, this->host));
		}
		this->runtime = runtime;

		return OpStatus::OK;
	}

	void CancelDialog()
	{
		if(has_dialog && wnd_cmd)
		{
			wnd_cmd->GetPermissionListener()->OnAskForPermissionCancelled(wnd_cmd, this);
			has_dialog = FALSE;
		}
	}

	virtual void OnAllowed(PersistenceType persistence)
	{
		Out();
		g_secman_instance->ProcessPendingRequests(SecurityManagerPermissionType(), GetHost(), IsPrivacyMode(), persistence, TRUE);
		ProcessChoice(persistence, TRUE);
	}

	virtual void OnDisallowed(PersistenceType persistence)
	{
		Out();
		g_secman_instance->ProcessPendingRequests(SecurityManagerPermissionType(), GetHost(), IsPrivacyMode(), persistence, FALSE);
		ProcessChoice(persistence, FALSE);
	}

	OpSecurityManager_UserConsent::PermissionType SecurityManagerPermissionType() { return type; }

	virtual PermissionType Type()
	{
		return OpSecurityManager_UserConsent::ToPermissionListenerPermission(type);
	}

	virtual const uni_char *GetURL()			{ return url.CStr(); }
	const char *GetHost()						{ return host.CStr(); }

	BOOL IsPrivacyMode() { return wnd_cmd->GetPrivacyMode(); }

	DOM_Runtime* Runtime() { return runtime; }

	/** Ask the user for consent.
	 *
	 * This method requests UI to ask the user for permission to run an
	 * operation.
	 *
	 * @param is_finished_synchronously Set to TRUE if the UI code responded
	 *        synchronously during this function call. May be NULL.
	 */
	void Ask(BOOL *is_finished_synchronously)
	{
		is_within_ask = TRUE;
		if(wnd_cmd)
			wnd_cmd->GetPermissionListener()->OnAskForPermission(wnd_cmd, this);
		is_within_ask = FALSE;

		has_dialog = TRUE;
		if (is_finished_synchronously)
			*is_finished_synchronously = this->is_finished_synchronously;

		if (this->is_finished_synchronously)
			OP_DELETE(this);
	}

	void ProcessChoice(PersistenceType persistence, BOOL allowed)
	{
		ChoicePersistenceType choice =  ToChoicePersistence(persistence);
		OpStatus::Ignore(persistence_provider->SetIsAllowed(allowed, choice));

		UserConsentRequest *request = m_requests.First();
		OP_ASSERT(request || !"There must be at least one request!");
		while (request)
		{
			UserConsentRequest *current_request = request;
			request = request->Suc();

			current_request->OnSecurityCheckSuccess(allowed, choice);
			OP_DELETE(current_request);
		}

		DeleteSelfIfAsync();
	}

private:
	friend class UserConsentRequest;

	/** Attach a UserConsentRequest to this dialog callback.
	 *
	 * The attached request will be notified of the answer given in the dialog.
	 * The ownership of the request is taken over by this dialog callback.
	 */
	void Attach(UserConsentRequest *request)
	{
		request->Into(&m_requests);
	}

	/** Inform the dialog callback that a request has been cancelled.
	 *
	 * If all requests associated with this dialog callback are cancelled,
	 * then the dialog is cancelled to and the callback itself is deleted.
	 *
	 * @param request The request that is being cancelled, it will be deleted.
	 */
	void OnRequestCancelled(UserConsentRequest *request)
	{
		OP_ASSERT(m_requests.HasLink(request));
		request->Out();
		OP_DELETE(request);
		if (m_requests.Empty())
		{
			CancelDialog();
			DeleteSelfIfAsync();
		}
	}

	/** Deletes the object.
	 *
	 * The object is deleted if this method is not called when executing the
	 * Ask method. If this method is called (indirectly) from within Ask then
	 * the is_fininshed_synchronously flag is set and Ask is responsible for
	 * deleting the object.
	 */
	void DeleteSelfIfAsync()
	{
		if (is_within_ask)
			is_finished_synchronously = TRUE;
		else
			OP_DELETE(this);
	}

	List<UserConsentRequest> m_requests;
	WindowCommander *wnd_cmd;
	OpSecurityPersistenceProvider *persistence_provider;
	OpString url;
	OpString8 host;
	DOM_Runtime *runtime;
	OpSecurityManager_UserConsent::PermissionType type;
	BOOL has_dialog;
	BOOL is_within_ask;
	BOOL is_finished_synchronously;
};

UserConsentRequest::UserConsentRequest(OpSecurityCheckCallback *security_check_callback, UserConsentDialogCallback *dialog_callback)
	: security_check_callback(security_check_callback), dialog_callback(dialog_callback)
{
	dialog_callback->Attach(this);
}

/* virtual */ void
UserConsentRequest::CancelSecurityCheck()
{
	dialog_callback->OnRequestCancelled(this);
}

///////////////////////////////////////////////////////////////////////////////
/// OpSecurityManager_UserConsent
///////////////////////////////////////////////////////////////////////////////

OpSecurityManager_UserConsent::OpSecurityManager_UserConsent()
{
}

/* virtual */
OpSecurityManager_UserConsent::~OpSecurityManager_UserConsent()
{
	for (unsigned int i = 0; i < PERMISSION_TYPE_MAX; ++i)
	{
		ClearSessionPermissions(static_cast<PermissionType>(i), FALSE);
		m_permission_callbacks[i].Clear();
		m_runtime_consent.DeleteAll();
	}
}

UserConsentType
OpSecurityManager_UserConsent::GetPermission(PermissionType permission_type, const uni_char *host, DOM_Runtime *runtime, BOOL privacy_mode, ChoicePersistenceType *persistence) const
{
	UserConsentType allowed = ASK;
	ChoicePersistenceType persistence_type = PERSISTENCE_NONE;

	if (!host)
		return DENIED;

	persistence_type = PERSISTENCE_RUNTIME;
	if (runtime)
		allowed = GetRuntimePermission(permission_type, runtime);
	if (allowed == ASK)
	{
		persistence_type = PERSISTENCE_SESSION;
		const OpStringHashTable<SessionConsentEntry> &session_permissions = privacy_mode ? m_privacy_mode_session_permissions[permission_type] : m_session_permissions[permission_type];
		SessionConsentEntry* consent;
		if (OpStatus::IsSuccess(session_permissions.GetData(host, &consent)))
			allowed = consent->is_allowed ? ALLOWED : DENIED;

		if (allowed == ASK && !privacy_mode)
		{
			persistence_type = PERSISTENCE_FULL;
			allowed = ToUserConsentType((BOOL3) g_pcjs->GetIntegerPref(PrefsForPermissionType(permission_type), host, TRUE));
		}
	}

	if (persistence)
		*persistence = persistence_type;

	return allowed;
}

#ifdef SECMAN_SET_USERCONSENT
static OP_STATUS
ToPermissionType(OpSecurityManager_UserConsent::PermissionType &permission, OpPermissionListener::PermissionCallback::PermissionType permission_type)
{
	switch(permission_type)
	{
#ifdef GEOLOCATION_SUPPORT
	case OpPermissionListener::PermissionCallback::PERMISSION_TYPE_GEOLOCATION_REQUEST:
		permission = OpSecurityManager_UserConsent::PERMISSION_TYPE_GEOLOCATION;
		break;
#endif // GEOLOCATION_SUPPORT
#ifdef DOM_STREAM_API_SUPPORT
	case OpPermissionListener::PermissionCallback::PERMISSION_TYPE_CAMERA_REQUEST:
		permission = OpSecurityManager_UserConsent::PERMISSION_TYPE_CAMERA;
		break;
#endif // DOM_STREAM_API_SUPPORT
	default:
		return OpStatus::ERR_OUT_OF_RANGE;
	}
	return OpStatus::OK;
}

OP_STATUS
OpSecurityManager_UserConsent::GetUserConsent(Window *window, OpVector<OpPermissionListener::ConsentInformation> &consent_information)
{
	for (unsigned int permission = 0; permission < PERMISSION_TYPE_MAX; ++permission)
		RETURN_IF_ERROR(GetUserConsent(window, static_cast<PermissionType>(permission), consent_information));
	return OpStatus::OK;
}

OP_STATUS
OpSecurityManager_UserConsent::GetUserConsent(Window *window, PermissionType permission, OpVector<OpPermissionListener::ConsentInformation> &consent_information)
{
	FramesDocument *frm_doc = window->GetCurrentDoc();
	if (!frm_doc)
		return OpStatus::ERR;

	BOOL privacy_mode = window->GetWindowCommander()->GetPrivacyMode();
	OpAutoStringHashTable<OpPermissionListener::ConsentInformation> consent_info_by_host;

	DocumentTreeIterator iterator(frm_doc);
	iterator.SetIncludeThis();
	while (iterator.Next())
	{
		const URL &url = iterator.GetFramesDocument()->GetOrigin()->security_context;
		const uni_char *host_name = GetHostName(url);
		if (!host_name)
			continue;

		// TODO: consider returning multiple entries per host in case there are
		// various settings (only possible with RUNTIME persistence).
		if (consent_info_by_host.Contains(host_name))
			continue;

		OpAutoPtr<OpPermissionListener::ConsentInformation> consent_anchor;

		DOM_Runtime *runtime = DOM_Utils::GetDOM_Runtime(iterator.GetFramesDocument()->GetESRuntime());
		ChoicePersistenceType frame_persistence;
		UserConsentType frame_is_allowed = GetPermission(permission, host_name, runtime, privacy_mode, &frame_persistence);
		if (frame_is_allowed != ASK)
		{
			OpPermissionListener::ConsentInformation *consent = OP_NEW(OpPermissionListener::ConsentInformation, ());
			RETURN_OOM_IF_NULL(consent);
			OpAutoPtr<OpPermissionListener::ConsentInformation> consent_anchor(consent);
			RETURN_IF_ERROR(consent->hostname.Set(host_name));

			consent->is_allowed = frame_is_allowed == ALLOWED;
			consent->persistence_type = ToPermissionListenerPersistence(frame_persistence);
			consent->permission_type = ToPermissionListenerPermission(permission);

			RETURN_IF_ERROR(consent_info_by_host.Add(consent->hostname.CStr(), consent));
			consent_anchor.release();
		}
	}

	RETURN_IF_ERROR(consent_info_by_host.CopyAllToVector(*((OpGenericVector *) &consent_information))); // The cast is necessary
	consent_info_by_host.RemoveAll();

	return OpStatus::OK;
}

OP_STATUS
OpSecurityManager_UserConsent::SetUserConsent(Window *window, OpPermissionListener::PermissionCallback::PermissionType permission_type, OpPermissionListener::PermissionCallback::PersistenceType persistence_type, const uni_char *hostname, UserConsentType allow)
{
	PermissionType permission;
	RETURN_IF_ERROR(ToPermissionType(permission, permission_type));

	ChoicePersistenceType persistence = ToChoicePersistence(persistence_type);
	if (persistence == PERSISTENCE_NONE)
		return OpStatus::ERR_OUT_OF_RANGE;

	if (persistence == PERSISTENCE_RUNTIME || allow == ASK)
	{
		DocumentTreeIterator iterator(window);
		iterator.SetIncludeThis();
		while (iterator.Next())
		{
			const URL &url = iterator.GetFramesDocument()->GetOrigin()->security_context;
			const uni_char *document_hostname = GetHostName(url);
			if (!document_hostname || !uni_str_eq(document_hostname, hostname))
				continue;

			DOM_Runtime *runtime = DOM_Utils::GetDOM_Runtime(iterator.GetFramesDocument()->GetESRuntime());
			if (runtime)
				RETURN_IF_ERROR(SetRuntimePermission(permission, runtime, allow));
		}
	}

	// Handle privacy mode
	BOOL privacy_mode = window->GetWindowCommander()->GetPrivacyMode();
	OpStringHashTable<SessionConsentEntry> &session_permissions = privacy_mode ? m_privacy_mode_session_permissions[permission] : m_session_permissions[permission];
	if (privacy_mode && persistence == PERSISTENCE_FULL)
		persistence = PERSISTENCE_SESSION;

	// Session permissions
	SessionConsentEntry *consent;
	if (persistence != PERSISTENCE_SESSION || allow == ASK)
	{
		if (OpStatus::IsSuccess(session_permissions.Remove(hostname, &consent)))
			OP_DELETE(consent);
	}
	else if (persistence == PERSISTENCE_SESSION && allow != ASK)
	{
		if (OpStatus::IsSuccess(session_permissions.GetData(hostname, &consent)))
			consent->is_allowed = allow == ALLOWED;
		else
			RETURN_IF_ERROR(AddSessionConsentEntry(&session_permissions, hostname, allow == ALLOWED));
	}

	if (persistence != PERSISTENCE_FULL || allow == ASK)
	{
		TRAPD(err, g_pcjs->RemoveOverrideL(hostname, PrefsForPermissionType(permission), TRUE));
		OpStatus::Ignore(err);
	}
	else if (persistence == PERSISTENCE_FULL && allow != ASK)
	{
		TRAPD(err, g_pcjs->OverridePrefL(hostname, PrefsForPermissionType(permission), allow == ALLOWED, TRUE));
		OpStatus::Ignore(err);
	}

	if (allow != ALLOWED)
	{
		NotifyUserConsentListenersForEachListener foreach_notify(hostname, permission);
		m_runtime_consent.ForEach(&foreach_notify);
	}

	return OpStatus::OK;
}
#endif // SECMAN_SET_USERCONSENT

void
OpSecurityManager_UserConsent::ClearUserConsentPermissions(PermissionType permission_type)
{
	ClearSessionPermissions(permission_type, FALSE);
	TRAPD(error, g_pcjs->RemoveOverrideL(NULL, PrefsForPermissionType(permission_type), TRUE));
	OpStatus::Ignore(error);

	ClearRuntimeConsentForEachListener foreach_listener(permission_type);
	m_runtime_consent.ForEach(&foreach_listener);
}

void
OpSecurityManager_UserConsent::OnExitPrivateMode()
{
	for (unsigned int i = 0; i < PERMISSION_TYPE_MAX; ++i)
		ClearSessionPermissions(static_cast<PermissionType>(i), TRUE);
}

void
OpSecurityManager_UserConsent::OnBeforeRuntimeDestroy(DOM_Runtime *runtime)
{
	RuntimeConsentEntry *consent_entry = NULL;
	OpStatus::Ignore(m_runtime_consent.Remove(runtime, &consent_entry));

	for (int permission = 0; permission < PERMISSION_TYPE_MAX; ++permission)
	{
		if (consent_entry)
			consent_entry->listeners[permission].NotifyBeforeRuntimeDestroyed(runtime);

		DisallowAllDialogs(&m_permission_callbacks[permission], runtime);
	}
	OP_DELETE(consent_entry);
}

void
OpSecurityManager_UserConsent::OnDocumentSuspend(FramesDocument *frm_doc)
{
	if (!frm_doc->GetDOMEnvironment())
		return; // If there are no scripts then there are no dialogs.

	DOM_Runtime *runtime = DOM_Utils::GetDOM_Runtime(frm_doc->GetDOMEnvironment()->GetRuntime());
	if (!runtime)
		return;

	for (int i = 0; i < PERMISSION_TYPE_MAX; ++i)
	{
		UserConsentDialogCallback *dialog_callback = m_permission_callbacks[i].First();
		if (dialog_callback)
		{
			List<UserConsentDialogCallback> *suspended_dialogs = NULL;
			m_suspended_dialogs.GetData(frm_doc, &suspended_dialogs);
			if (!suspended_dialogs)
			{
				suspended_dialogs = OP_NEW(List<UserConsentDialogCallback>, ());
				OP_STATUS status = m_suspended_dialogs.Add(frm_doc, suspended_dialogs);
				if (OpStatus::IsError(status))
				{
					OP_DELETE(suspended_dialogs);
					DisallowAllDialogs(&m_permission_callbacks[i], runtime);
					return;
				}
			}
			if (!suspended_dialogs)
			{
				DisallowAllDialogs(&m_permission_callbacks[i], runtime);
				return;
			}

			while (dialog_callback)
			{
				UserConsentDialogCallback *next_dialog_callback = dialog_callback->Suc();
				if (dialog_callback->Runtime() == runtime)
				{
					dialog_callback->CancelDialog();
					dialog_callback->Out();
					dialog_callback->Into(suspended_dialogs);
				}
				dialog_callback = next_dialog_callback;
			}
		}
	}
}

void
OpSecurityManager_UserConsent::OnDocumentResume(FramesDocument *frm_doc)
{
	List<UserConsentDialogCallback> *suspended_dialogs = NULL;
	m_suspended_dialogs.GetData(frm_doc, &suspended_dialogs);
	if (suspended_dialogs)
	{
		UserConsentDialogCallback *dialog_callback = suspended_dialogs->First();
		while (dialog_callback)
		{
			UserConsentDialogCallback *next_dialog_callback = dialog_callback->Suc();
			dialog_callback->Out();
			dialog_callback->Into(&(m_permission_callbacks[dialog_callback->SecurityManagerPermissionType()]));
			dialog_callback->Ask(NULL);
			dialog_callback = next_dialog_callback;
		}
	}
}

void
OpSecurityManager_UserConsent::OnDocumentDestroy(FramesDocument *frm_doc)
{
	List<UserConsentDialogCallback> *suspended_dialogs = NULL;
	m_suspended_dialogs.Remove(frm_doc, &suspended_dialogs);
	if (suspended_dialogs)
		DisallowAllDialogs(suspended_dialogs, NULL);

	OP_DELETE(suspended_dialogs);
}

OP_STATUS
OpSecurityManager_UserConsent::SetPermission(PermissionType permission_type, const uni_char *host, DOM_Runtime *runtime, ChoicePersistenceType persistence_type, BOOL privacy_mode, BOOL allow)
{
	OP_ASSERT(permission_type < PERMISSION_TYPE_MAX);

	if (privacy_mode && persistence_type == PERSISTENCE_FULL)
		persistence_type = PERSISTENCE_SESSION;

	switch (persistence_type)
	{
	case PERSISTENCE_SESSION:
		{
			OpStringHashTable<SessionConsentEntry> &session_permissions = privacy_mode ? m_privacy_mode_session_permissions[permission_type] : m_session_permissions[permission_type];
			SessionConsentEntry *consent;
			if (OpStatus::IsSuccess(session_permissions.GetData(host, &consent)))
				consent->is_allowed = allow;
			else
				RETURN_IF_ERROR(AddSessionConsentEntry(&session_permissions, host, allow));
		}
		break;
	case PERSISTENCE_FULL:
		TRAPD(err, g_pcjs->OverridePrefL(host, PrefsForPermissionType(permission_type), allow, TRUE));
		break; // Don't cache, otherwise incorrect persistence type is returned by IsAllowed
	case PERSISTENCE_RUNTIME:
		OP_ASSERT(runtime);
		SetRuntimePermission(permission_type, runtime, allow ? ALLOWED : DENIED);
		break;
	}

	return OpStatus::OK;
}

void
OpSecurityManager_UserConsent::ClearSessionPermissions(PermissionType permission_type, BOOL privacy_mode)
{
	OpStringHashTable<SessionConsentEntry> &session_permissions = privacy_mode ? m_privacy_mode_session_permissions[permission_type] : m_session_permissions[permission_type];
	session_permissions.DeleteAll();
}

OP_STATUS
OpSecurityManager_UserConsent::CheckUserConsentSecurity(const OpSecurityContext& source, OpSecurityPersistenceProvider* persistence_provider, PermissionType permission_type, OpSecurityCheckCallback *callback, OpSecurityCheckCancel **cancel)
{
	UserConsentType allowed = DENIED;
	ChoicePersistenceType persistence_type;

	OP_ASSERT(DOM_Utils::GetWorkerOwnerDocument(source.GetRuntime()) == NULL && "User consent security checks don't support WebWorkers yet.");

	if (source.GetWindow()->IsThumbnailWindow())
	{
		OP_DELETE(persistence_provider);
		callback->OnSecurityCheckSuccess(FALSE);
		return OpStatus::OK;
	}

	allowed = persistence_provider->IsAllowed(&persistence_type);

	if (allowed != ASK)
	{
		OP_DELETE(persistence_provider);
		callback->OnSecurityCheckSuccess(allowed == ALLOWED ? TRUE : FALSE, persistence_type);
	}
	else
		return AskForConfirmation(source, persistence_provider, permission_type, callback, cancel);

	return OpStatus::OK;
}

#ifdef GEOLOCATION_SUPPORT
OP_STATUS
OpSecurityManager_UserConsent::CheckGeolocationSecurity(const OpSecurityContext& source, OpSecurityCheckCallback *callback, OpSecurityCheckCancel **cancel)
{
	OpSecurityPersistenceProvider* persistence_provider;
	RETURN_IF_ERROR(UserConsentPersistenceProvider::Create(&persistence_provider, PERMISSION_TYPE_GEOLOCATION, source));

	return CheckUserConsentSecurity(source, persistence_provider, PERMISSION_TYPE_GEOLOCATION, callback, cancel);
}
#endif // GEOLOCATION_SUPPORT

#ifdef DOM_STREAM_API_SUPPORT
OP_STATUS
OpSecurityManager_UserConsent::CheckCameraSecurity(const OpSecurityContext& source, OpSecurityCheckCallback *callback, OpSecurityCheckCancel **cancel)
{
	OpSecurityPersistenceProvider* persistence_provider;
	RETURN_IF_ERROR(UserConsentPersistenceProvider::Create(&persistence_provider, PERMISSION_TYPE_CAMERA, source));

	return CheckUserConsentSecurity(source, persistence_provider, PERMISSION_TYPE_CAMERA, callback, cancel);
}
#endif // DOM_STREAM_API_SUPPORT

OP_STATUS
OpSecurityManager_UserConsent::AskForConfirmation(const OpSecurityContext& source,
												  OpSecurityPersistenceProvider *persistence_provider,
												  PermissionType permission_type,
												  OpSecurityCheckCallback* security_callback,
												  OpSecurityCheckCancel** cancel)
{
	WindowCommander* win_cmd = source.GetWindow()->GetWindowCommander();

	OpAutoPtr<OpSecurityPersistenceProvider> ap_persistence_provider(persistence_provider);

	UserConsentDialogCallback *dialog_callback = m_permission_callbacks[permission_type].First();
	while(dialog_callback)
	{
		if (dialog_callback->Runtime() == source.GetRuntime())
			break;
		dialog_callback = dialog_callback->Suc();
	}

	OpAutoPtr<UserConsentDialogCallback> ap_dialog_callback(NULL);
	BOOL callback_is_new = FALSE;
	if (!dialog_callback)
	{
		dialog_callback = OP_NEW(UserConsentDialogCallback, (win_cmd, persistence_provider, permission_type));
		RETURN_OOM_IF_NULL(dialog_callback);
		ap_persistence_provider.release();
		ap_dialog_callback.reset(dialog_callback);
		RETURN_IF_ERROR(dialog_callback->Construct(source.GetURL(), source.GetRuntime()));
		callback_is_new = TRUE;
	}

	UserConsentRequest *request = OP_NEW(UserConsentRequest, (security_callback, dialog_callback));
	RETURN_OOM_IF_NULL(request);

	if (callback_is_new)
	{
		ap_dialog_callback.release();
		dialog_callback->Into(&m_permission_callbacks[permission_type]);

		BOOL is_finished_synchronously;
		dialog_callback->Ask(&is_finished_synchronously); // The listener might finish the operation synchronously and delete the dialog_callback
		if (!is_finished_synchronously && cancel)
			*cancel = request;
	}

	return OpStatus::OK;
}

void
OpSecurityManager_UserConsent::ProcessPendingRequests(PermissionType permission_type, const char *host, BOOL privacy_mode, OpPermissionListener::PermissionCallback::PersistenceType persistence_type, BOOL allowed)
{
	if (persistence_type == OpPermissionListener::PermissionCallback::PERSISTENCE_TYPE_SESSION ||
	    persistence_type == OpPermissionListener::PermissionCallback::PERSISTENCE_TYPE_ALWAYS)
	{
		UserConsentDialogCallback *obj = m_permission_callbacks[permission_type].First();
		while (host && obj)
		{
			UserConsentDialogCallback *suc = obj->Suc();

			const char* h = obj->GetHost();
			if (permission_type == obj->SecurityManagerPermissionType() && obj->IsPrivacyMode() == privacy_mode && h && op_strcmp(h, host) == 0)
			{
				obj->CancelDialog();
				obj->ProcessChoice(persistence_type, allowed);
			}

			obj = suc;
		}
	}
}

/* static */ void
OpSecurityManager_UserConsent::DisallowAllDialogs(const List<UserConsentDialogCallback> *dialog_callbacks, DOM_Runtime *runtime)
{
	UserConsentDialogCallback *dialog_callback = dialog_callbacks->First();
	while (dialog_callback)
	{
		if (!runtime || runtime == dialog_callback->Runtime())
		{
			dialog_callback->CancelDialog();
			dialog_callback->OnDisallowed(OpPermissionListener::PermissionCallback::PERSISTENCE_TYPE_NONE);
			dialog_callback = dialog_callbacks->First();
		}
		else
			dialog_callback = dialog_callback->Suc();
	}
}

void
OpSecurityManager_UserConsent::NotifyUserConsentListeners(PermissionType permission_type, DOM_Runtime *runtime)
{
	OP_ASSERT(permission_type < PERMISSION_TYPE_MAX);
	RuntimeConsentEntry *consent_entry;
	OP_STATUS status = m_runtime_consent.GetData(runtime, &consent_entry);
	if (OpStatus::IsSuccess(status))
		consent_entry->listeners[permission_type].NotifyUserConsentRevoked();
}

OP_STATUS
OpSecurityManager_UserConsent::RegisterUserConsentListener(OpSecurityManager_UserConsent::PermissionType permission_type, DOM_Runtime *runtime, UserConsentListener *listener)
{
	OP_ASSERT(permission_type < PERMISSION_TYPE_MAX);
	RuntimeConsentEntry *consent_entry;
	OP_STATUS status = m_runtime_consent.GetData(runtime, &consent_entry);
	if (OpStatus::IsError(status))
		RETURN_IF_ERROR(CreateRuntimeConsentEntry(consent_entry, runtime));

	return consent_entry->listeners[permission_type].AttachListener(listener);
}

void
OpSecurityManager_UserConsent::UnregisterUserConsentListener(OpSecurityManager_UserConsent::PermissionType permission_type, DOM_Runtime *runtime, UserConsentListener *listener)
{
	OP_ASSERT(permission_type < PERMISSION_TYPE_MAX);
	RuntimeConsentEntry *consent_entry;
	OP_STATUS status = m_runtime_consent.GetData(runtime, &consent_entry);
	if (OpStatus::IsSuccess(status))
		consent_entry->listeners[permission_type].DetachListener(listener);
}

UserConsentType
OpSecurityManager_UserConsent::GetRuntimePermission(PermissionType permission_type, DOM_Runtime *runtime) const
{
	RuntimeConsentEntry *consent_entry;
	OP_STATUS status = m_runtime_consent.GetData(runtime, &consent_entry);
	if (OpStatus::IsSuccess(status))
		return consent_entry->consent[permission_type];
	else
		return ASK;
}

OP_STATUS
OpSecurityManager_UserConsent::SetRuntimePermission(PermissionType permission_type, DOM_Runtime *runtime, UserConsentType consent)
{
	RuntimeConsentEntry *consent_entry;
	OP_STATUS status = m_runtime_consent.GetData(runtime, &consent_entry);
	if (OpStatus::IsError(status))
	{
		if  (consent != ASK)
		{
			RETURN_IF_ERROR(CreateRuntimeConsentEntry(consent_entry, runtime));
			consent_entry->consent[permission_type] = consent;
		}
	}
	else
		consent_entry->consent[permission_type] = consent;

	return OpStatus::OK;
}

/* static */ OpPermissionListener::PermissionCallback::PermissionType
OpSecurityManager_UserConsent::ToPermissionListenerPermission(PermissionType permission_type)
{
	switch (permission_type)
	{
#ifdef GEOLOCATION_SUPPORT
	case PERMISSION_TYPE_GEOLOCATION:
		return OpPermissionListener::PermissionCallback::PERMISSION_TYPE_GEOLOCATION_REQUEST;
#endif // GEOLOCATION_SUPPORT
#ifdef DOM_STREAM_API_SUPPORT
	case PERMISSION_TYPE_CAMERA:
		return OpPermissionListener::PermissionCallback::PERMISSION_TYPE_CAMERA_REQUEST;
#endif // DOM_STREAM_API_SUPPORT
	default:
		OP_ASSERT(!"Unsupported permission type");
		return OpPermissionListener::PermissionCallback::PERMISSION_TYPE_UNKNOWN;
	}
}

/* static */ PrefsCollectionJS::integerpref
OpSecurityManager_UserConsent::PrefsForPermissionType(PermissionType permission_type)
{
	switch (permission_type)
	{
#ifdef GEOLOCATION_SUPPORT
	case PERMISSION_TYPE_GEOLOCATION:
		return PrefsCollectionJS::AllowGeolocation;
#endif // GEOLOCATION_SUPPORT
#ifdef DOM_STREAM_API_SUPPORT
	case PERMISSION_TYPE_CAMERA:
		return PrefsCollectionJS::AllowCamera;
#endif // DOM_STREAM_API_SUPPORT
	default:
		OP_ASSERT(!"Unsupported permission type");
		return PrefsCollectionJS::DummyLastIntegerPref;
	}
}

OP_STATUS
OpSecurityManager_UserConsent::AddSessionConsentEntry(OpStringHashTable<OpSecurityManager_UserConsent::SessionConsentEntry> *session_permissions, const uni_char *hostname, BOOL is_allowed)
{
	SessionConsentEntry *consent_entry = OP_NEW(SessionConsentEntry, ());
	RETURN_OOM_IF_NULL(consent_entry);
	OpAutoPtr<SessionConsentEntry> consent_anchor(consent_entry);
	RETURN_IF_ERROR(consent_entry->hostname.Set(hostname));
	consent_entry->is_allowed = is_allowed;
	RETURN_IF_ERROR(session_permissions->Add(consent_entry->hostname.CStr(), consent_entry));
	consent_anchor.release();
	return OpStatus::OK;
}

OP_STATUS
OpSecurityManager_UserConsent::CreateRuntimeConsentEntry(RuntimeConsentEntry *&consent_entry, DOM_Runtime *runtime)
{
	consent_entry = OP_NEW(RuntimeConsentEntry, ());
	RETURN_OOM_IF_NULL(consent_entry);
	OP_STATUS status = m_runtime_consent.Add(runtime, consent_entry);
	if (OpStatus::IsError(status))
		OP_DELETE(consent_entry);

	return status;
}

#ifdef SECMAN_SET_USERCONSENT
/* virtual */ void
OpSecurityManager_UserConsent::NotifyUserConsentListenersForEachListener::HandleKeyData(const void* key, void* data)
{
	const DOM_Runtime *runtime = static_cast<const DOM_Runtime *>(key);
	RuntimeConsentEntry *consent_entry = static_cast<RuntimeConsentEntry *>(data);

	const URL &url = DOM_Utils::GetDocument(const_cast<DOM_Runtime *>(runtime))->GetOrigin()->security_context;
	const uni_char *hostname = GetHostName(url);
	if (hostname && uni_str_eq(hostname, m_hostname))
		consent_entry->listeners[m_permission_type].NotifyUserConsentRevoked();
}
#endif // SECMAN_SET_USERCONSENT

/* virtual */ void
OpSecurityManager_UserConsent::ClearRuntimeConsentForEachListener::HandleKeyData(const void* key, void* data)
{
	RuntimeConsentEntry *consent_entry = static_cast<RuntimeConsentEntry *>(data);
	consent_entry->consent[m_permission_type] = ASK;

	consent_entry->listeners[m_permission_type].NotifyUserConsentRevoked();
}

/* static */ const uni_char *
OpSecurityManager_UserConsent::GetHostName(const URL &url)
{
	const uni_char *hostname = url.GetServerName() ? url.GetServerName()->UniName() : NULL;
#ifdef SELFTEST
	if (!hostname && g_selftest.running && url.GetAttribute(URL::KProtocolName) == "opera" && url.GetAttribute(URL::KPath) == "blanker")
		hostname = GetSelftestHostName();
#endif // SELFTEST
	return hostname;
}

#endif // SECMAN_USERCONSENT
