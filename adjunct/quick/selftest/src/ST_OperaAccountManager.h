/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#ifndef __ST_OPERA_ACCOUNT_MANAGER__
#define __ST_OPERA_ACCOUNT_MANAGER__

#include "adjunct/quick/managers/OperaAccountManager.h"

class ST_OperaAccountManager : public OperaAccountManager, public OperaAccountController::OAC_Listener
{
public:
	enum SuccessCriteria
	{
		SuccessCriteriaAuthentication,
		SuccessCriteriaDeviceRegistration,
		SuccessCriteriaDeviceRegistrationForced,
		SuccessCriteriaDeviceRelease
	};


	ST_OperaAccountManager(SuccessCriteria success_criteria)
		: m_success_criteria(success_criteria)
	{
		AddListener(this);
	}
	virtual ~ST_OperaAccountManager()
	{
		RemoveListener(this);
	}

	OP_STATUS SetPrefsUserName(const OpStringC & username)
	{
		return m_prefs_username.Set(username);
	}

	OP_STATUS SetWandPassword(const OpStringC & password)
	{
		return m_wand_password.Set(password);
	}

protected:
	virtual OP_STATUS OAuthAuthenticate(const OpStringC& username, const OpStringC& password, const OpStringC& device_name, const OpStringC& install_id, BOOL force = FALSE)
	{
		switch(m_success_criteria)
		{
		case SuccessCriteriaAuthentication:
			{
				if (username.HasContent() && password.HasContent())
				{
					ST_passed();
					return OpStatus::OK;
				}
			}
			break;
		case SuccessCriteriaDeviceRegistration:
		case SuccessCriteriaDeviceRegistrationForced:
			{
				if (username.HasContent() && password.HasContent() && device_name.HasContent() && install_id.HasContent())
				{
					if (m_success_criteria == SuccessCriteriaDeviceRegistrationForced && force == FALSE)
					{
						ST_failed("No forced device creation");
					}
					else
					{
						ST_passed();
					}
					return OpStatus::OK;
				}
			}
			break;
		default:
			{
				ST_failed("Wrong action or invalid data");
			}
		}
		return OpStatus::OK;
	}

	virtual OP_STATUS OAuthReleaseDevice(const OpStringC& username, const OpStringC& password, const OpStringC& devicename)
	{
		if (m_success_criteria == SuccessCriteriaDeviceRelease)
		{
			if (username.HasContent() && password.HasContent() && devicename.HasContent())
			{
				ST_passed();
				return OpStatus::OK;
			}
		}
		ST_failed("Wrong action or invalid data");
		return OpStatus::OK;
	}

	virtual void OnOperaAccountDeviceCreate(OperaAuthError error, const OpStringC& shared_secret, const OpStringC& server_message) {}

	virtual OP_STATUS GetUserNameFromPrefs(OpString & username) const
	{
		RETURN_IF_ERROR(username.Set(m_prefs_username));
		return OpStatus::OK;
	}

	virtual OP_STATUS GetWandPassword(const OpStringC8 &protocol, const OpStringC &server, const OpStringC &username, PrivacyManagerCallback* callback)
	{
		if (protocol.Compare(WAND_OPERA_ACCOUNT) != 0 ||
			server.HasContent() ||
			username.IsEmpty() ||
			callback == NULL)
		{
			ST_failed("invalid wand information");
		}

		if (m_wand_password.HasContent())
		{
			OnPasswordRetrieved(m_wand_password);
		}
		else
		{
			OnPasswordFailed();
		}
		return OpStatus::OK;
	}

	virtual void OnPasswordMissing()
	{
		if (m_current_task == CurrentTaskNone)
		{
			ST_failed("password retrieval without current task");
		}
		else
		{
			ST_passed();
		}
	}

private:
	SuccessCriteria	m_success_criteria;
	OpString		m_prefs_username;
	OpString		m_wand_password;
};

#endif // __ST_OPERA_ACCOUNT_MANAGER__
