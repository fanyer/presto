#include "core/pch.h"

#ifdef DAPI_JIL_NETWORKRESOURCES_SUPPORT

#include "modules/device_api/jil/JILNetworkResources.h"
#include "modules/url/url_api.h"
#include "modules/xmlutils/xmlfragment.h"
#include "modules/xmlutils/xmlnames.h"
#include "modules/pi/device_api/OpSubscriberInfo.h"
#include "modules/device_api/jil/JILNetworkRequestHandler.h"

class AuthenticationRequestHandler: public JILNetworkRequestHandler
{
public:
	AuthenticationRequestHandler(): JILNetworkRequestHandler(NULL) {}
	
	OP_STATUS SendRequest()
	{
		OpString8 request, msisdn;
		RETURN_IF_ERROR(g_op_subscriber_info->GetSubscriberMSISDN(&msisdn));	
		RETURN_IF_ERROR(OpStatus::IsError(request.AppendFormat("\
			<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\
			<Request>\
			   <operation>GetSecurityToken</operation>\
			   <Authentication> \
				  <msisdn>%s</msisdn>\
			   </Authentication>\
			</Request>", msisdn.CStr())));

		return SendRequestInternal("csg/1.0/authentication", request.CStr());
	}

	const char *GetSecurityToken()
	{
		return m_securityToken.CStr();
	}

protected:
	OP_STATUS HandleServerError(int code)
	{
		if (code == SE_INVALID_TOKEN)
		{
			// This response is invalid for an authentication request.
			ChangeState(STATE_FAILED);
			return OpStatus::ERR;
		}
		else
			return JILNetworkRequestHandler::HandleServerError(code);
	}

	OP_STATUS TakeResult(XMLFragment &fragment)
	{
		if (fragment.EnterElement(UNI_L("Response")) && fragment.EnterElement(UNI_L("SecurityResponse")) && fragment.EnterElement(UNI_L("token")))
		{
			TempBuffer buffer;
			RETURN_IF_ERROR(fragment.GetAllText(buffer));
			RETURN_IF_ERROR(m_securityToken.SetUTF8FromUTF16(buffer.GetStorage(), static_cast<int>(buffer.Length())));
			return OpStatus::OK;
		}
		return OpStatus::ERR;
	}
};

class UserAccountBalanceHandler: public JILNetworkRequestHandler
{
public:
	UserAccountBalanceHandler(JilNetworkResources::GetUserAccountBalanceCallback *callback): JILNetworkRequestHandler(callback) {}

	OP_STATUS SendRequest()
	{
		OpString8 request, msisdn;
		RETURN_IF_ERROR(g_op_subscriber_info->GetSubscriberMSISDN(&msisdn));	
		RETURN_IF_ERROR(request.AppendFormat("\
			<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\
			<Request>\
				<operation>GetCustomerBalancesInfo</operation>\
				<securityToken>%s</securityToken>\
				<CustomerBalancesRequest>\
					<msisdn>%s</msisdn>\
					<detailsInfo>false</detailsInfo>\
				</CustomerBalancesRequest>\
			</Request>", m_securityToken.CStr(), msisdn.CStr()));
	
		return SendRequestInternal("csg/1.0/tariffinfo", request.CStr());
	}

protected:
	OP_STATUS TakeResult(XMLFragment &fragment)
	{
		if (fragment.EnterElement(UNI_L("Response")) && fragment.EnterElement(UNI_L("CustomerBalancesResponse")) && fragment.EnterElement(UNI_L("currency")))
		{
			TempBuffer buffer;
			RETURN_IF_ERROR(fragment.GetAllText(buffer));
			RETURN_IF_ERROR(static_cast<JilNetworkResources::GetUserAccountBalanceCallback*>(m_callback)->SetCurrency(buffer.GetStorage()));
			fragment.LeaveElement();
			if (fragment.EnterElement(UNI_L("cash")))
			{
				buffer.Clear();
				RETURN_IF_ERROR(fragment.GetAllText(buffer));
				double cash;
				RETURN_IF_ERROR(StringToDouble(buffer.GetStorage(), cash));
				static_cast<JilNetworkResources::GetUserAccountBalanceCallback*>(m_callback)->SetCash(cash);
				return OpStatus::OK;
			}			
		}
		return OpStatus::ERR;
	}
};

class UserSubscriptionTypeHandler: public JILNetworkRequestHandler
{
public:
	UserSubscriptionTypeHandler(JilNetworkResources::GetUserSubscriptionTypeCallback *callback): JILNetworkRequestHandler(callback) {}

	OP_STATUS SendRequest()
	{
		OpString8 request, msisdn;
		RETURN_IF_ERROR(g_op_subscriber_info->GetSubscriberMSISDN(&msisdn));
		RETURN_IF_ERROR(request.AppendFormat("\
			<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\
			<Request>\
				<operation>GetCustomerTariffInfo</operation>\
				<securityToken>%s</securityToken>\
				<CustomerTariffInfoRequest>\
					<msisdn>%s</msisdn>\
				</CustomerTariffInfoRequest>\
			</Request>", m_securityToken.CStr(), msisdn.CStr()));
	
		return SendRequestInternal("csg/1.0/tariffinfo", request.CStr());
	}

protected:
	OP_STATUS TakeResult(XMLFragment &fragment)
	{
		if (fragment.EnterElement(UNI_L("Response")) && fragment.EnterElement(UNI_L("CustomerTariffInfoReponse")) 
			&& fragment.EnterElement(UNI_L("Tariff")) && fragment.EnterElement(UNI_L("prepay")))
		{
			TempBuffer buffer;
			RETURN_IF_ERROR(fragment.GetAllText(buffer));
			RETURN_IF_ERROR(static_cast<JilNetworkResources::GetUserSubscriptionTypeCallback*>(m_callback)->SetType( 
				uni_stricmp(buffer.GetStorage(),"true") == 0 ? UNI_L("prepaid") : UNI_L("postpaid")));
			return OpStatus::OK;
		}
		return OpStatus::ERR;
	}
};

class SelfLocationHandler: public JILNetworkRequestHandler
{
public:
	SelfLocationHandler(JilNetworkResources::GetSelfLocationCallback *callback): JILNetworkRequestHandler(callback) {}

	OP_STATUS SendRequest()
	{
		OpString8 request, msisdn;
		RETURN_IF_ERROR(g_op_subscriber_info->GetSubscriberMSISDN(&msisdn));	
		RETURN_IF_ERROR(request.AppendFormat("\
			<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\
			<Request>\
				<operation>GetSelfLocation</operation>\
				<securityToken>%s</securityToken>\
				<Location>\
					<msisdn>%s</msisdn>\
				</Location>\
			</Request>", m_securityToken.CStr(), msisdn.CStr()));
	
		return SendRequestInternal("csg/1.0/location", request.CStr());
	}

protected:
	OP_STATUS TakeResult(XMLFragment &fragment)
	{
		int cellId;
		double latitude, longitude, accuracy;

		if (!fragment.EnterElement(UNI_L("Response")) || !fragment.EnterElement(UNI_L("LocationResponse")) || !fragment.EnterElement(UNI_L("cellId")))
			return OpStatus::ERR;
		TempBuffer buffer;
		RETURN_IF_ERROR(fragment.GetAllText(buffer));
		RETURN_IF_ERROR(StringToInt(buffer.GetStorage(), cellId));
		static_cast<JilNetworkResources::GetSelfLocationCallback*>(m_callback)->SetCellId(cellId);

		fragment.LeaveElement();
		if (!fragment.EnterElement(UNI_L("latitude")))
			return OpStatus::ERR;
		buffer.Clear();
		RETURN_IF_ERROR(fragment.GetAllText(buffer));
		RETURN_IF_ERROR(StringToDouble(buffer.GetStorage(), latitude));
		static_cast<JilNetworkResources::GetSelfLocationCallback*>(m_callback)->SetLatitude(latitude);

		fragment.LeaveElement();
		if (!fragment.EnterElement(UNI_L("longitude")))
			return OpStatus::ERR;
		buffer.Clear();
		RETURN_IF_ERROR(fragment.GetAllText(buffer));
		RETURN_IF_ERROR(StringToDouble(buffer.GetStorage(), longitude));
		static_cast<JilNetworkResources::GetSelfLocationCallback*>(m_callback)->SetLongitude(longitude);

		fragment.LeaveElement();
		if (!fragment.EnterElement(UNI_L("accuracy")))
			return OpStatus::ERR;
		buffer.Clear();
		RETURN_IF_ERROR(fragment.GetAllText(buffer));
		RETURN_IF_ERROR(StringToDouble(buffer.GetStorage(), accuracy));
		static_cast<JilNetworkResources::GetSelfLocationCallback*>(m_callback)->SetAccuracy(accuracy);

		return OpStatus::OK;
	}
};

JilNetworkResources::JilNetworkResources(): m_authRequest(NULL)
{
}

OP_STATUS JilNetworkResources::Construct()
{
	g_main_message_handler->SetCallBack(this, MSG_DAPI_JIL_HANDLER_STATUS_CHANGED, 0);
	return OpStatus::OK;
}

JilNetworkResources::~JilNetworkResources()
{
	g_main_message_handler->UnsetCallBacks(this);
	m_requests.Clear();
}

OP_STATUS JilNetworkResources::GetSecurityToken()
{
	m_authRequest = OP_NEW(AuthenticationRequestHandler,());
	RETURN_OOM_IF_NULL(m_authRequest);
	m_authRequest->Into(&m_requests);

	OP_STATUS res;
	if (OpStatus::IsError(res = m_authRequest->SendRequest()))
	{
		OP_DELETE(m_authRequest);
		return res;
	}
	return OpStatus::OK;
}

void JilNetworkResources::PrepareTokenRequiringRequest(JILNetworkRequestHandlerCallback *callback, JILNetworkRequestHandler *handler)
{
	OP_STATUS res = OpStatus::ERR_NO_MEMORY;
	if (!handler || m_securityToken.IsEmpty() && OpStatus::IsError(res = GetSecurityToken()))
	{
		OP_DELETE(handler);
		callback->Finished(res);
		return;
	}

	handler->Into(&m_requests);
	if ( !m_securityToken.IsEmpty() && 
			(OpStatus::IsError(res = handler->SetSecurityToken(m_securityToken.CStr())) || OpStatus::IsError(res = handler->SendRequest())))
	{
		OP_DELETE(handler);
		callback->Finished(res);
	}
}

void JilNetworkResources::GetUserAccountBalance(GetUserAccountBalanceCallback *callback)
{
	PrepareTokenRequiringRequest(callback, OP_NEW(UserAccountBalanceHandler,(callback)));
}

void JilNetworkResources::GetUserSubscriptionType(GetUserSubscriptionTypeCallback *callback)
{
	PrepareTokenRequiringRequest(callback, OP_NEW(UserSubscriptionTypeHandler,(callback)));
}

void JilNetworkResources::GetSelfLocation(GetSelfLocationCallback *callback)
{
	PrepareTokenRequiringRequest(callback, OP_NEW(SelfLocationHandler,(callback)));
}

void JilNetworkResources::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if (msg==MSG_DAPI_JIL_HANDLER_STATUS_CHANGED)
	{
		JILNetworkRequestHandler *handler = reinterpret_cast<JILNetworkRequestHandler*>(par1);
		
		switch(handler->GetState())
		{
			case JILNetworkRequestHandler::STATE_FINISHED:
				if (handler==m_authRequest)
				{
					m_securityToken.Set(m_authRequest->GetSecurityToken());
					for (JILNetworkRequestHandler *it = static_cast<JILNetworkRequestHandler*>(m_requests.First()); it!=NULL; it = static_cast<JILNetworkRequestHandler*>(it->Suc()))
					{
						if (it->GetState() == JILNetworkRequestHandler::STATE_NEW || it->GetState() == JILNetworkRequestHandler::STATE_NEW_TOKEN_NEEDED)
						{
							it->SetSecurityToken(m_securityToken.CStr());
							it->SendRequest();
						}
					}
					m_authRequest = NULL;
				}
				OP_DELETE(handler);
			break;
			case JILNetworkRequestHandler::STATE_FAILED:
				if (handler==m_authRequest)
					for (JILNetworkRequestHandler *it = static_cast<JILNetworkRequestHandler*>(m_requests.First()); it!=NULL; it = static_cast<JILNetworkRequestHandler*>(it->Suc()))
						if (it != handler)
							it->FinishCallBackAndChangeState(OpStatus::ERR);
				OP_DELETE(handler);
			break;
			case JILNetworkRequestHandler::STATE_NEW_TOKEN_NEEDED:
			{
				OP_STATUS res;
				if (OpStatus::IsError(res = GetSecurityToken()))
					handler->FinishCallBackAndChangeState(res);
			}
			break;
		}
	}
}

#endif // DOM_JIL_API_SUPPORT
