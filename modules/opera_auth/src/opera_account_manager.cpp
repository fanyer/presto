/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2010-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef OPERA_AUTH_SUPPORT

#include "modules/opera_auth/opera_auth_myopera.h"
#include "modules/opera_auth/opera_account_manager.h"
#include "modules/formats/hex_converter.h"
#include "modules/prefs/prefsmanager/opprefslistener.h"
#include "modules/prefs/prefsmanager/collections/pc_webserver.h"
#include "modules/prefs/prefsmanager/collections/pc_opera_account.h"
#include "modules/util/opguid.h"
#include "modules/webserver/webserver-api.h"

#define OPERA_ACCOUNT_WAND_ID UNI_L("opera:account")


class CoreOperaAccountManagerImpl
	: public CoreOperaAccountManager
	, public OperaAuthentication::Listener
	, public WandLoginCallback
	, public OpPrefsListener
{
public:
	CoreOperaAccountManagerImpl();
	void ConstructL();
	virtual ~CoreOperaAccountManagerImpl();

	virtual OpStringC& GetUsername();
	virtual BOOL IsLoggedIn();
	virtual BOOL GetSavePasswordSetting();

#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
	virtual OpStringC& GetDevice();
	virtual OpStringC& GetSharedSecret();
	virtual BOOL IsDeviceRegistered() { return GetDevice().HasContent() && GetSharedSecret().HasContent(); }
	virtual OP_STATUS ConditionallySetWebserverUser();
#endif

	virtual BOOL IsRequestInProgress() { return m_currentRequest != NoRequest; }
	virtual OperaAccountError LastError() { return m_lastError; }

	virtual void CreateAccount(const OpStringC& username, const OpStringC& password, const OpStringC& email, BOOL savePassword);
	virtual void Login(const OpStringC& username, const OpStringC& password, BOOL savePassword);
	virtual void ChangePassword(const OpStringC& username, const OpStringC& old_password, const OpStringC& new_password, BOOL savePassword);
#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
	virtual void RegisterDevice(const OpStringC &device, BOOL force);
	virtual void ReleaseDevice(BOOL userInitiated);
#endif
	virtual void Abort();
	virtual void Logout();

	virtual OP_STATUS AddListener(CoreOperaAccountManager::Listener* listener);
	virtual void RemoveListener(CoreOperaAccountManager::Listener* listener);

	virtual OP_STATUS GetLoginPassword(Window* window, WandLoginCallback* callback);

	// OperaAuthentication::Listener callbacks
	virtual void OnAuthCreateAccount(OperaAuthError error, OperaRegistrationInformation& reg_info);
	virtual void OnAuthAuthenticate(OperaAuthError error, const OpStringC& shared_secret, const OpStringC& server_message, const OpStringC& token);
	virtual void OnAuthPasswordChanged(OperaAuthError error);
	virtual void OnAuthCreateDevice(OperaAuthError error, const OpStringC& shared_secret, const OpStringC& server_message);
	virtual void OnAuthReleaseDevice(OperaAuthError error);

	// WandLoginCallback callbacks
	virtual void OnPasswordRetrieved(const uni_char* password);
	virtual void OnPasswordRetrievalFailed();

	// OpPrefsListener callback
	virtual void PrefChanged(OpPrefsCollection::Collections id, int pref, int newvalue);
	virtual void PrefChanged(OpPrefsCollection::Collections id, int pref, const OpStringC &newvalue);

private:
	enum RequestType {
		NoRequest,
		CreateAccountRequest,
		LoginRequest,
		ChangeUserPassword,
#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
		RegisterDeviceRequest,
		ReleaseDeviceRequest,
#endif
		LogoutRequest
	};

	BOOL SetCurrentRequest(RequestType request);
	void FinishRequest(OperaAccountError status);
	void FinishRequest(OP_STATUS status);
	void CallListeners_LoginStateChange();
#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
	void CallListeners_DeviceRegisteredChange();
#endif
	void RetrievePassword();
	OP_STATUS SetAttemptedUsernamePassword(const OpStringC& username, const OpStringC& password, BOOL savePassword);
	OP_STATUS SaveVerifiedPassword();
	void ForgetPassword();
	void ForgetSavedPassword();

#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
	void DoRegisterDevice();
	void DoReleaseDevice();
	OP_STATUS SaveSharedSecret(const OpStringC& shared_secret);
	void ForgetSharedSecret();
#endif
	OperaAccountError TranslateOperaAuthError(OperaAuthError error);

	class ListenerHolder : public ListElement<ListenerHolder>
	{
	public:
		CoreOperaAccountManager::Listener* m_listener;
		ListenerHolder(CoreOperaAccountManager::Listener* listener) : m_listener(listener) {}
	};

	OpString m_username;
	OpString m_attemptedUsername;
	OpString m_attemptedPassword;
	OpString m_verifiedUsername;
	OpString m_verifiedPassword;

	RequestType m_currentRequest;

	#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
	OpString m_device;
	BOOL m_force;
	OpString m_sharedSecret;
#endif
	OperaAccountError m_lastError;
	OperaAccountError m_pendingError;
	AutoDeleteList<ListenerHolder> m_listeners;
	MyOperaAuthentication m_my_opera_authentication;
};

void CoreOperaAccountManager::CreateL(CoreOperaAccountManager*& operaAccountManager)
{
	operaAccountManager = NULL;
	CoreOperaAccountManagerImpl *operaAccountManagerImpl = OP_NEW_L(CoreOperaAccountManagerImpl, ());
	TRAPD(status, operaAccountManagerImpl->ConstructL());
	if (OpStatus::IsError(status))
	{
		OP_DELETE(operaAccountManagerImpl);
		LEAVE(status);
	}
	operaAccountManager = operaAccountManagerImpl;
}

CoreOperaAccountManagerImpl::CoreOperaAccountManagerImpl()
	: 	m_currentRequest(NoRequest)
#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
	,	m_force(FALSE)
#endif
	,	m_lastError(OPERA_ACCOUNT_SUCCESS)
	,	m_pendingError(OPERA_ACCOUNT_SUCCESS)

{
	m_my_opera_authentication.SetListener(this);
	m_my_opera_authentication.SetTimeout(3*60);
}

void CoreOperaAccountManagerImpl::ConstructL()
{
	g_pcopera_account->RegisterListenerL(this);
}

CoreOperaAccountManagerImpl::~CoreOperaAccountManagerImpl()
{
	g_pcopera_account->UnregisterListener(this);
}

OpStringC& CoreOperaAccountManagerImpl::GetUsername()
{
	OpStatus::Ignore(m_username.Set(g_pcopera_account->GetStringPref(PrefsCollectionOperaAccount::Username)));
	return m_username;
}

#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
OpStringC& CoreOperaAccountManagerImpl::GetDevice()
{
	OpStatus::Ignore(m_device.Set(g_pcwebserver->GetStringPref(PrefsCollectionWebserver::WebserverDevice)));
	return m_device;
}

OpStringC& CoreOperaAccountManagerImpl::GetSharedSecret()
{
	OpStatus::Ignore(m_sharedSecret.Set(g_pcwebserver->GetStringPref(PrefsCollectionWebserver::WebserverHashedPassword)));
	return m_sharedSecret;
}
#endif // WEBSERVER_RENDEZVOUS_SUPPORT

BOOL CoreOperaAccountManagerImpl::GetSavePasswordSetting()
{
	return g_pcopera_account->GetIntegerPref(PrefsCollectionOperaAccount::SavePassword) != 0;
}

BOOL CoreOperaAccountManagerImpl::IsLoggedIn()
{
	OpStringC& username = GetUsername();
	if (username.Compare(m_verifiedUsername) == 0 && m_verifiedPassword.HasContent())
		return TRUE;
	WandLogin *info = g_wand_manager->FindLogin(OPERA_ACCOUNT_WAND_ID, 0);
	return info && info->username.Compare(username) == 0;
}


#define FINISH_REQUEST_IF_ERROR(expr) \
	do { \
		OP_STATUS CALLBACK_RETURN_IF_ERROR_TMP = expr; \
		if (OpStatus::IsError(CALLBACK_RETURN_IF_ERROR_TMP)) { \
			FinishRequest(CALLBACK_RETURN_IF_ERROR_TMP); \
			return; \
		} \
	} while (0)

#define FINISH_REQUEST_IF_LEAVE(expr) \
	do { \
		TRAPD(CALLBACK_RETURN_IF_LEAVE_TMP, expr); \
		if (OpStatus::IsError(CALLBACK_RETURN_IF_LEAVE_TMP)) { \
			FinishRequest(CALLBACK_RETURN_IF_LEAVE_TMP); \
			return; \
		} \
	} while (0)

#define FINISH_REQUEST_IF_LEAVE_OR_ERROR(expr) \
	FINISH_REQUEST_IF_LEAVE( \
		FINISH_REQUEST_IF_ERROR(expr))

#define RETURN_IF_LEAVE_OR_ERROR(expr) \
	RETURN_IF_LEAVE(RETURN_IF_ERROR(expr))


void CoreOperaAccountManagerImpl::CreateAccount(const OpStringC& username,const OpStringC& password, const OpStringC& email, BOOL savePassword)
{
	if (!SetCurrentRequest(CreateAccountRequest))
		return;

	if (IsLoggedIn())
	{
		FinishRequest(OPERA_ACCOUNT_ALREADY_LOGGED_IN);
		return;
	}

	ForgetPassword();

	FINISH_REQUEST_IF_ERROR(SetAttemptedUsernamePassword(username, password, savePassword));

	OperaRegistrationInformation reg_info;
	FINISH_REQUEST_IF_ERROR(reg_info.m_username.Set(username));
	FINISH_REQUEST_IF_ERROR(reg_info.m_password.Set(password));
	FINISH_REQUEST_IF_ERROR(reg_info.m_email.Set(email));

	FINISH_REQUEST_IF_ERROR(m_my_opera_authentication.CreateAccount(reg_info));
	if (IsRequestInProgress()) // CreateAccount may already have called OnAuthCreateAccount
		CallListeners_LoginStateChange();
}

// OperaAuthentication::Callback callback
void CoreOperaAccountManagerImpl::OnAuthCreateAccount(OperaAuthError error, OperaRegistrationInformation& reg_info)
{
	OP_ASSERT(m_currentRequest == CreateAccountRequest);
	if (m_currentRequest != CreateAccountRequest)
		return;

	if (error == AUTH_OK)
		FINISH_REQUEST_IF_ERROR(SaveVerifiedPassword());
	FinishRequest(TranslateOperaAuthError(error));
}

void CoreOperaAccountManagerImpl::Login(const OpStringC& username, const OpStringC& password, BOOL savePassword)
{
	if (!SetCurrentRequest(LoginRequest))
		return;

	if (IsLoggedIn())
	{
		FinishRequest(OPERA_ACCOUNT_ALREADY_LOGGED_IN);
		return;
	}

	ForgetPassword();

	FINISH_REQUEST_IF_ERROR(SetAttemptedUsernamePassword(username, password, savePassword));

	OpStringC empty;
	FINISH_REQUEST_IF_ERROR(m_my_opera_authentication.Authenticate(username, password, empty, empty, false));
	if (IsRequestInProgress()) // Authenticate may already have called OnAuthAuthenticate
		CallListeners_LoginStateChange();
}
void CoreOperaAccountManagerImpl::ChangePassword(const OpStringC& username, const OpStringC& old_password, const OpStringC& new_password, BOOL savePassword)
{
	if (!SetCurrentRequest(ChangeUserPassword))
		return;

	FINISH_REQUEST_IF_ERROR(SetAttemptedUsernamePassword(username.CStr(), new_password.CStr(), savePassword));

	FINISH_REQUEST_IF_ERROR(m_my_opera_authentication.ChangePassword(username, old_password, new_password));
	if (IsRequestInProgress()) // Authenticate may already have called OnAuthAuthenticate
		CallListeners_LoginStateChange();
}

// OperaAuthentication::Callback callback
void CoreOperaAccountManagerImpl::OnAuthAuthenticate(OperaAuthError error, const OpStringC& shared_secret, const OpStringC& server_message, const OpStringC& token)
{
	OP_ASSERT(m_currentRequest == LoginRequest);
	if (m_currentRequest != LoginRequest)
		return;

	// Currently, nothing uses "token", it is ignored
	if (error == AUTH_OK)
		FINISH_REQUEST_IF_ERROR(SaveVerifiedPassword());
	FinishRequest(TranslateOperaAuthError(error));
}

void CoreOperaAccountManagerImpl::OnAuthPasswordChanged(OperaAuthError error)
{
	OP_ASSERT(m_currentRequest == ChangeUserPassword);
	if (m_currentRequest != ChangeUserPassword)
		return;

	if (error == AUTH_OK)
		FINISH_REQUEST_IF_ERROR(SaveVerifiedPassword());
	FinishRequest(TranslateOperaAuthError(error));

}

#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
void CoreOperaAccountManagerImpl::RegisterDevice(const OpStringC &device, BOOL force)
{
	if (!SetCurrentRequest(RegisterDeviceRequest))
		return;

	if (!IsLoggedIn())
	{
		FinishRequest(OPERA_ACCOUNT_NOT_LOGGED_IN);
		return;
	}

	ForgetSharedSecret();

	FINISH_REQUEST_IF_ERROR(m_attemptedUsername.Set(GetUsername()));
	FINISH_REQUEST_IF_ERROR(m_device.Set(device));
	m_force = force;
	FINISH_REQUEST_IF_LEAVE_OR_ERROR(g_pcwebserver->WriteStringL(PrefsCollectionWebserver::WebserverDevice, device));
	FINISH_REQUEST_IF_LEAVE_OR_ERROR(g_pcwebserver->WriteStringL(PrefsCollectionWebserver::WebserverUser, m_attemptedUsername));

	if (m_verifiedPassword.HasContent())
		DoRegisterDevice();
	else
		RetrievePassword();
}

void CoreOperaAccountManagerImpl::DoRegisterDevice()
{
	OP_ASSERT(m_currentRequest == RegisterDeviceRequest);
	if (m_currentRequest != RegisterDeviceRequest)
		return;

	OP_ASSERT(m_verifiedUsername.HasContent());
	OP_ASSERT(m_verifiedPassword.HasContent());
	OP_ASSERT(m_device.HasContent());
	OpString computer_id;
	FINISH_REQUEST_IF_ERROR(computer_id.Set(g_pcopera_account->GetStringPref(PrefsCollectionOperaAccount::ComputerID)));
#if defined(PREFS_WRITE) && defined(UTIL_GUID_GENERATE_SUPPORT)
	OpGuid guid;
	if (computer_id.IsEmpty() &&
		OpStatus::IsSuccess(g_opguidManager->GenerateGuid(guid)) &&
		OpStatus::IsSuccess(HexConverter::AppendAsHex(computer_id, &guid, sizeof(guid), HexConverter::UseUppercase)))
		// for compatibility reason with Opera Link, the generated UUID MUST be upper case!
	{
		TRAPD(status,
			  OpStatus::Ignore(g_pcopera_account->WriteStringL(PrefsCollectionOperaAccount::ComputerID, computer_id)));
		OpStatus::Ignore(status); // If we can't make it, just give up
	}
#endif
	FINISH_REQUEST_IF_ERROR(m_my_opera_authentication.Authenticate(m_verifiedUsername, m_verifiedPassword, m_device, computer_id, m_force));
	if (IsRequestInProgress()) // Authenticate may already have called OnAuthCreateDevice
		CallListeners_DeviceRegisteredChange();
}
#endif // WEBSERVER_RENDEZVOUS_SUPPORT

// OperaAuthentication::Callback callback
void CoreOperaAccountManagerImpl::OnAuthCreateDevice(OperaAuthError error, const OpStringC& shared_secret, const OpStringC& server_message)
{
#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
	OP_ASSERT(m_currentRequest == RegisterDeviceRequest);
	if (m_currentRequest != RegisterDeviceRequest)
		return;

	if (error == AUTH_OK)
		SaveSharedSecret(shared_secret);
	else if (error == AUTH_ACCOUNT_AUTH_FAILURE)
		ForgetPassword(); // i.e. Logout
	FinishRequest(TranslateOperaAuthError(error));
#endif
}

#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
void CoreOperaAccountManagerImpl::ReleaseDevice(BOOL userInitiated /* = TRUE */)
{
	if (!IsDeviceRegistered())
		return;

	ForgetSharedSecret(); // This "releases" the device internally

	if (!SetCurrentRequest(ReleaseDeviceRequest))
		return;

	if (!userInitiated)
	{
		FinishRequest(OPERA_ACCOUNT_SUCCESS);
		return;
	}

	if (!IsLoggedIn())
	{
		FinishRequest(OPERA_ACCOUNT_NOT_LOGGED_IN);
		return;
	}

	if (m_verifiedPassword.HasContent())
		DoReleaseDevice();
	else
	{
		FINISH_REQUEST_IF_ERROR(m_attemptedUsername.Set(GetUsername()));
		RetrievePassword();
	}
}

void CoreOperaAccountManagerImpl::DoReleaseDevice()
{
	OP_ASSERT(m_currentRequest == ReleaseDeviceRequest);
	if (m_currentRequest != ReleaseDeviceRequest)
		return;

	OP_ASSERT(m_verifiedUsername.HasContent());
	OP_ASSERT(m_verifiedPassword.HasContent());
	OP_ASSERT(m_device.HasContent());
	FINISH_REQUEST_IF_ERROR(m_my_opera_authentication.ReleaseDevice(m_verifiedUsername, m_verifiedPassword, m_device));
}
#endif // WEBSERVER_RENDEZVOUS_SUPPORT

// OperaAuthentication::Callback callback
void CoreOperaAccountManagerImpl::OnAuthReleaseDevice(OperaAuthError error)
{
#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
	OP_ASSERT(m_currentRequest == ReleaseDeviceRequest);
	if (m_currentRequest != ReleaseDeviceRequest)
		return;

	if (error != AUTH_OK)
		ForgetPassword(); // i.e. Logout
	FinishRequest(TranslateOperaAuthError(error));
#endif
}

void CoreOperaAccountManagerImpl::RetrievePassword()
{
	WandLogin *info = g_wand_manager->FindLogin(OPERA_ACCOUNT_WAND_ID, 0);
	if (!info || info->username.Compare(m_attemptedUsername) != 0)
		OnPasswordRetrievalFailed();
	else
		FINISH_REQUEST_IF_ERROR(g_wand_manager->GetLoginPasswordWithoutWindow(info, this));
}

// WandLoginCallback callback
void CoreOperaAccountManagerImpl::OnPasswordRetrieved(const uni_char* password)
{
	OP_ASSERT(m_attemptedUsername.HasContent());
	FINISH_REQUEST_IF_ERROR(m_verifiedUsername.Set(m_attemptedUsername));
	FINISH_REQUEST_IF_ERROR(m_verifiedPassword.Set(password));
	m_attemptedUsername.Empty();

#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
	if (m_currentRequest == RegisterDeviceRequest)
		DoRegisterDevice();
	else if (m_currentRequest == ReleaseDeviceRequest)
		DoReleaseDevice();
#endif
}

// WandLoginCallback callback
void CoreOperaAccountManagerImpl::OnPasswordRetrievalFailed()
{
	ForgetPassword(); // i.e. Logout
#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
	if (m_currentRequest == ReleaseDeviceRequest)
		ForgetSharedSecret();
#endif
	FinishRequest(OPERA_ACCOUNT_INVALID_USERNAME_PASSWORD);
}

void CoreOperaAccountManagerImpl::Abort()
{
	if (!IsRequestInProgress())
		return;

	if (m_my_opera_authentication.OperationInProgress())
		m_my_opera_authentication.Abort(TRUE);
	else
		FinishRequest(OPERA_ACCOUNT_REQUEST_ABORTED);
}

void CoreOperaAccountManagerImpl::Logout()
{
	if (!IsLoggedIn())
		return;

	if (!SetCurrentRequest(LogoutRequest))
		return;

	ForgetPassword();
	FinishRequest(OPERA_ACCOUNT_SUCCESS);
}

OP_STATUS CoreOperaAccountManagerImpl::AddListener(CoreOperaAccountManager::Listener* listener)
{
	RemoveListener(listener); // Just in case
	ListenerHolder* holder = OP_NEW(ListenerHolder, (listener));
	RETURN_OOM_IF_NULL(holder);
	holder->Into(&m_listeners);
	return OpStatus::OK;
}

void CoreOperaAccountManagerImpl::RemoveListener(CoreOperaAccountManager::Listener* listener)
{
	for (ListenerHolder* holder = m_listeners.First(); holder; holder = holder->Suc())
		if (holder->m_listener == listener)
		{
			holder->Out();
			OP_DELETE(holder);
			return;
		}
}

OP_STATUS CoreOperaAccountManagerImpl::GetLoginPassword(Window* window, WandLoginCallback* callback)
{
	if (!callback)
		return OpStatus::ERR;

	OpStringC& username = GetUsername();
	if (username.Compare(m_verifiedUsername) == 0 && m_verifiedPassword.HasContent())
	{
		callback->OnPasswordRetrieved(m_verifiedPassword.CStr());
		return OpStatus::OK;
	}

	WandLogin *info = g_wand_manager->FindLogin(OPERA_ACCOUNT_WAND_ID, 0);
	if (!info || info->username.Compare(username) != 0)
	{
		callback->OnPasswordRetrievalFailed();
		return OpStatus::OK;
	}

	if (window)
		return g_wand_manager->GetLoginPassword(window, info, callback);

	return g_wand_manager->GetLoginPasswordWithoutWindow(info, callback);
}

BOOL CoreOperaAccountManagerImpl::SetCurrentRequest(RequestType request)
{
	OP_ASSERT(request != NoRequest);
	if (IsRequestInProgress())
	{
		m_lastError = OPERA_ACCOUNT_REQUEST_ALREADY_IN_PROGRESS;
		return FALSE;
	}
	m_currentRequest = request;
	m_lastError = OPERA_ACCOUNT_SUCCESS;
	return TRUE;
}

void CoreOperaAccountManagerImpl::FinishRequest(OperaAccountError status)
{
	OP_ASSERT(IsRequestInProgress());
	m_lastError = status;

#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
	if (m_currentRequest == RegisterDeviceRequest || m_currentRequest == ReleaseDeviceRequest)
	{
		BOOL auth_failure = !IsLoggedIn() && m_currentRequest == RegisterDeviceRequest && status != OPERA_ACCOUNT_NOT_LOGGED_IN;
		m_currentRequest = NoRequest;
		g_webserver->ChangeListeningMode(WEBSERVER_LISTEN_DEFAULT);
		CallListeners_DeviceRegisteredChange();
		if (auth_failure) // Authentication failure caused a logout
			CallListeners_LoginStateChange();
		return;
	}
#endif
	m_currentRequest = NoRequest;
	CallListeners_LoginStateChange();
}

void CoreOperaAccountManagerImpl::FinishRequest(OP_STATUS status)
{
	if (OpStatus::IsMemoryError(status))
		FinishRequest(OPERA_ACCOUNT_OUT_OF_MEMORY);
	else if (OpStatus::IsError(status))
		FinishRequest(OPERA_ACCOUNT_INTERNAL_CLIENT_ERROR);
	else
		FinishRequest(OPERA_ACCOUNT_SUCCESS);
}

void CoreOperaAccountManagerImpl::CallListeners_LoginStateChange()
{
	// Then do the actual calls
	for (ListenerHolder* holder = m_listeners.First(); holder; holder = holder->Suc())
		holder->m_listener->OnAccountLoginStateChange();
}

#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
void CoreOperaAccountManagerImpl::CallListeners_DeviceRegisteredChange()
{
	// Then do the actual calls
	for (ListenerHolder* holder = m_listeners.First(); holder; holder = holder->Suc())
		holder->m_listener->OnAccountDeviceRegisteredChange();
}

OP_STATUS CoreOperaAccountManagerImpl::ConditionallySetWebserverUser()
{
	if (IsLoggedIn() &&
		g_pcwebserver->GetIntegerPref(PrefsCollectionWebserver::WebserverEnable) != 0 &&
		g_pcwebserver->GetStringPref(PrefsCollectionWebserver::WebserverUser).IsEmpty())
	{
		TRAPD(status, status = g_pcwebserver->WriteStringL(PrefsCollectionWebserver::WebserverUser, GetUsername()));
		return status;
	}
	return OpStatus::OK;
}
#endif

OP_STATUS CoreOperaAccountManagerImpl::SetAttemptedUsernamePassword(const OpStringC& username, const OpStringC& password, BOOL savePassword)
{
	RETURN_IF_ERROR(m_attemptedUsername.Set(username));
	RETURN_IF_ERROR(m_attemptedPassword.Set(password));
	RETURN_IF_LEAVE_OR_ERROR(g_pcopera_account->WriteIntegerL(PrefsCollectionOperaAccount::SavePassword, savePassword ? 1 : 0));
	return OpStatus::OK;
}

OP_STATUS CoreOperaAccountManagerImpl::SaveVerifiedPassword()
{
	OP_NEW_DBG("CoreOperaAccountManagerImpl::SaveVerifiedPassword()", "opera.auth");
	OP_ASSERT(m_attemptedUsername.HasContent());
	OP_ASSERT(m_attemptedPassword.HasContent());
	RETURN_IF_ERROR(m_verifiedUsername.Set(m_attemptedUsername));
	RETURN_IF_ERROR(m_verifiedPassword.Set(m_attemptedPassword));
	m_attemptedUsername.Empty();
	m_attemptedPassword.Empty();

	if (m_verifiedUsername.Compare(GetUsername()) != 0)
		RETURN_IF_LEAVE_OR_ERROR(g_pcopera_account->WriteStringL(PrefsCollectionOperaAccount::Username, m_verifiedUsername));
#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
	ConditionallySetWebserverUser();
#endif

	if (!GetSavePasswordSetting())
		return OpStatus::OK;

	// Store the password
	return g_wand_manager->StoreLoginWithoutWindow(OPERA_ACCOUNT_WAND_ID, m_verifiedUsername.CStr(), m_verifiedPassword.CStr());
}

void CoreOperaAccountManagerImpl::ForgetPassword()
{
	m_verifiedUsername.Empty();
	m_verifiedPassword.Empty();
	ForgetSavedPassword();
}

void CoreOperaAccountManagerImpl::ForgetSavedPassword()
{
	for (;;)
	{
		WandLogin *info = g_wand_manager->FindLogin(OPERA_ACCOUNT_WAND_ID, 0);
		if (!info)
			break;
		g_wand_manager->DeleteLogin(OPERA_ACCOUNT_WAND_ID, info->username.CStr());
	}
}

#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
OP_STATUS CoreOperaAccountManagerImpl::SaveSharedSecret(const OpStringC& shared_secret)
{
	if (shared_secret.Compare(GetSharedSecret()) != 0)
	{
		RETURN_IF_ERROR(m_sharedSecret.Set(shared_secret));
		RETURN_IF_LEAVE_OR_ERROR(g_pcwebserver->WriteStringL(PrefsCollectionWebserver::WebserverHashedPassword, shared_secret));
	}
	return OpStatus::OK;
}

void CoreOperaAccountManagerImpl::ForgetSharedSecret()
{
	m_sharedSecret.Empty();
	TRAPD(status, g_pcwebserver->ResetStringL(PrefsCollectionWebserver::WebserverHashedPassword));
	OpStatus::Ignore(status);
}
#endif // WEBSERVER_RENDEZVOUS_SUPPORT

// OpPrefsListener callback
void CoreOperaAccountManagerImpl::PrefChanged(OpPrefsCollection::Collections id, int pref, int newvalue)
{
	if (IsRequestInProgress())
		return; // The pref change was ours

	if (id == OpPrefsCollection::OperaAccount && pref == PrefsCollectionOperaAccount::SavePassword && newvalue == 0)
		ForgetSavedPassword();
}

// OpPrefsListener callback
void CoreOperaAccountManagerImpl::PrefChanged(OpPrefsCollection::Collections id, int pref, const OpStringC &newvalue)
{
	if (IsRequestInProgress())
		return; // The pref change was ours

	if (id == OpPrefsCollection::OperaAccount && pref == PrefsCollectionOperaAccount::Username)
	{
		m_lastError = OPERA_ACCOUNT_SUCCESS;
		CallListeners_LoginStateChange();
	}
}

CoreOperaAccountManager::OperaAccountError CoreOperaAccountManagerImpl::TranslateOperaAuthError(OperaAuthError error)
{
	switch (error)
	{
		case AUTH_OK:								return OPERA_ACCOUNT_SUCCESS;
		default:
		case AUTH_ERROR:							return OPERA_ACCOUNT_INTERNAL_CLIENT_ERROR;
		case AUTH_ERROR_SERVER:						return OPERA_ACCOUNT_INTERNAL_SERVER_ERROR;
		case AUTH_ERROR_PARSER:						return OPERA_ACCOUNT_PARSING_ERROR;
		case AUTH_ERROR_MEMORY:						return OPERA_ACCOUNT_OUT_OF_MEMORY;
		case AUTH_ERROR_COMM_FAIL:					return OPERA_ACCOUNT_REQUEST_ERROR;
		case AUTH_ERROR_COMM_ABORTED:				return OPERA_ACCOUNT_REQUEST_ABORTED;
		case AUTH_ERROR_COMM_TIMEOUT:				return OPERA_ACCOUNT_REQUEST_TIMEOUT;
		case AUTH_ERROR_INVALID_REQUEST:			return OPERA_ACCOUNT_BAD_REQUEST;
		case AUTH_ACCOUNT_CREATE_USER_EXISTS:		return OPERA_ACCOUNT_USER_ALREADY_EXISTS;
		case AUTH_ACCOUNT_CREATE_DEVICE_EXISTS:		return OPERA_ACCOUNT_DEVICE_NAME_ALREADY_IN_USE;
		case AUTH_ACCOUNT_CREATE_DEVICE_INVALID:	return OPERA_ACCOUNT_DEVICE_NAME_INVALID;
		case AUTH_ACCOUNT_CREATE_USER_EMAIL_ERROR:	return OPERA_ACCOUNT_EMAIL_ADDRESS_INVALID;
		case AUTH_ACCOUNT_CREATE_USER_EMAIL_EXISTS:	return OPERA_ACCOUNT_EMAIL_ADDRESS_ALREADY_IN_USE;
		case AUTH_ACCOUNT_CREATE_USER_TOO_SHORT:	return OPERA_ACCOUNT_USERNAME_TOO_SHORT;
		case AUTH_ACCOUNT_CREATE_USER_TOO_LONG:		return OPERA_ACCOUNT_USERNAME_TOO_LONG;
		case AUTH_ACCOUNT_CREATE_USER_INVALID_CHARS:return OPERA_ACCOUNT_USERNAME_CONTAINS_INVALID_CHARACTERS;
		case AUTH_ACCOUNT_CREATE_PASSWORD_TOO_SHORT:return OPERA_ACCOUNT_PASSWORD_TOO_SHORT;
		case AUTH_ACCOUNT_CREATE_PASSWORD_TOO_LONG:	return OPERA_ACCOUNT_PASSWORD_TOO_LONG;
		case AUTH_ACCOUNT_AUTH_FAILURE:				return OPERA_ACCOUNT_INVALID_USERNAME_PASSWORD;
		case AUTH_ACCOUNT_AUTH_INVALID_KEY:			return OPERA_ACCOUNT_INVALID_ENCRYPTION_KEY_USED;
		case AUTH_ACCOUNT_USER_BANNED:				return OPERA_ACCOUNT_USER_BANNED;
		case AUTH_ACCOUNT_CHANGE_PASSWORD_TO_MANY_CHANGES: return OPERA_ACCOUNT_TO_MANY_CHANGES;
	}
}

#undef OPERA_ACCOUNT_WAND_ID
#undef FINISH_REQUEST_IF_ERROR
#undef FINISH_REQUEST_IF_LEAVE
#undef FINISH_REQUEST_IF_LEAVE_OR_ERROR
#undef RETURN_IF_LEAVE_OR_ERROR

#endif // OPERA_AUTH_SUPPORT
