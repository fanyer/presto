#include "core/pch.h"

#ifdef DAPI_JIL_NETWORKRESOURCES_SUPPORT

#include "modules/device_api/jil/JILNetworkRequestHandler.h"
#include "modules/device_api/jil/JILNetworkResources.h"
#include "modules/xmlutils/xmlfragment.h"
#include "modules/xmlutils/xmlnames.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"

JILNetworkRequestHandler::JILNetworkRequestHandler(JilNetworkResources::JILNetworkRequestHandlerCallback *callback)
	: m_callback(callback), m_retryCount(0), m_state(STATE_NEW)
{
	m_retryTimer.SetTimerListener(this);
}

JILNetworkRequestHandler::~JILNetworkRequestHandler() 
{
	Out();
	g_main_message_handler->UnsetCallBacks(this);
}

JILNetworkRequestHandler::State JILNetworkRequestHandler::GetState() 
{
	return m_state; 
}

OP_STATUS JILNetworkRequestHandler::SetSecurityToken( const char* securityToken)
{
	return m_securityToken.Set(securityToken);
}

void JILNetworkRequestHandler::FinishCallBackAndChangeState(OP_STATUS status)
{
	if (m_callback)
		m_callback->Finished(status);
	ChangeState(OpStatus::IsSuccess(status) ? STATE_FINISHED : STATE_FAILED);
}

void JILNetworkRequestHandler::ChangeState(State state)
{
	m_state = state;
	g_main_message_handler->PostMessage(MSG_DAPI_JIL_HANDLER_STATUS_CHANGED, reinterpret_cast<MH_PARAM_1>(this), 0);
}

OP_STATUS JILNetworkRequestHandler::SendRequestInternal(const char *path, const char *xmlContent)
{
	OpString hostName;
	OpString8 hostName8, url;
	RETURN_IF_LEAVE(g_prefsManager->GetPreferenceL("JIL API", "JIL Network Resources Host Name", hostName));
	RETURN_IF_ERROR(hostName8.SetUTF8FromUTF16(hostName));
	RETURN_IF_ERROR(url.AppendFormat("http://%s/%s", hostName8.CStr(), path));
	m_url = g_url_api->GetURL(url.CStr(), "", TRUE);

	RETURN_IF_LEAVE(m_url.SetAttribute(URL::KHTTP_Method, HTTP_METHOD_POST));
	RETURN_IF_LEAVE(m_url.SetHTTP_Data(xmlContent, TRUE));
	RETURN_IF_LEAVE(m_url.SetHTTP_ContentType("text/xml"));
	RETURN_IF_LEAVE(SetLoadingCallbacks(m_url.Id()));

	URL_LoadPolicy loadpolicy(FALSE, URL_Load_Normal, FALSE);
	CommState state = m_url.LoadDocument(g_main_message_handler, URL(), loadpolicy);

	if (state != COMM_LOADING)
		return OpStatus::ERR;

	ChangeState(STATE_WAITING);
	return OpStatus::OK;
}

OP_STATUS JILNetworkRequestHandler::StringToInt(const uni_char *s, int &i)
{
	uni_char *end;
	i = uni_strtol(s, &end, 10, NULL);
	return s==end ? OpStatus::ERR : OpStatus::OK;
}

OP_STATUS JILNetworkRequestHandler::StringToDouble(const uni_char *s, double &d)
{
	uni_char *end;
	d = uni_strtod(s, &end);
	return s==end ? OpStatus::ERR : OpStatus::OK;
}

OP_STATUS JILNetworkRequestHandler::SetLoadingCallbacks(URL_ID url_id)
{
	MessageHandler* mh = g_main_message_handler;

	if (!mh->HasCallBack(this, MSG_URL_DATA_LOADED, (MH_PARAM_1) url_id))
	if (mh->SetCallBack(this, MSG_URL_DATA_LOADED, (MH_PARAM_1) url_id) == OpStatus::ERR_NO_MEMORY)
		return OpStatus::ERR_NO_MEMORY;

	if (!mh->HasCallBack(this, MSG_URL_LOADING_FAILED, (MH_PARAM_1) url_id))
	if (mh->SetCallBack(this, MSG_URL_LOADING_FAILED, (MH_PARAM_1) url_id) == OpStatus::ERR_NO_MEMORY)
		return OpStatus::ERR_NO_MEMORY;

	return OpStatus::OK;
}

void JILNetworkRequestHandler::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if (m_state==STATE_FAILED || m_state==STATE_FINISHED)
		return;
	switch (msg)
	{
		case MSG_URL_DATA_LOADED:
			if (m_url.Id()==(URL_ID)par1 && m_url.GetAttribute(URL::KLoadStatus) == URL_LOADED)
			{
				XMLFragment fragment;
				OP_STATUS res;

				if (OpStatus::IsError(res = fragment.Parse(m_url)))
					FinishCallBackAndChangeState(res);
				else
				{
					if (OpStatus::IsError(res = TakeResult(fragment)))
					{
						int errorCode;
						if (OpStatus::IsSuccess(TakeErrorCode(fragment, errorCode)) && OpStatus::IsSuccess(HandleServerError(errorCode)))
							return;
						if (m_callback)
							NotifyCallbackWithErrorDescription(fragment);
					}
					FinishCallBackAndChangeState(res);
				}
			}
			break;
		case MSG_URL_LOADING_FAILED:
			FinishCallBackAndChangeState(OpStatus::ERR);
			break;
	}
}

OP_STATUS JILNetworkRequestHandler::TakeErrorCode(XMLFragment &fragment, int &code)
{
	fragment.RestartFragment();
	if (!fragment.EnterElement(UNI_L("Response")) || !fragment.EnterElement(UNI_L("ErrorResponse")) || !fragment.EnterElement(UNI_L("errorCode")))
		return OpStatus::ERR;

	TempBuffer buffer;
	RETURN_IF_ERROR(fragment.GetAllText(buffer));
	RETURN_IF_ERROR(StringToInt(buffer.GetStorage(), code));
	return OpStatus::OK;
}

OP_STATUS JILNetworkRequestHandler::HandleServerError(int code)
{
	switch (code)
	{
		case SE_INVALID_TOKEN:
			ChangeState(STATE_NEW_TOKEN_NEEDED);
			return OpStatus::OK;
		case SE_TRY_AGAIN:
			if (m_retryCount<JIL_NETWORK_REQUEST_TRY_COUNT)
			{
				m_retryTimer.Start(JIL_NETWORK_REQUEST_TRY_INTERVAL);
				return OpStatus::OK;
			}
			return OpStatus::ERR;
		default:
			return OpStatus::ERR_NOT_SUPPORTED;
	}
}

void JILNetworkRequestHandler::NotifyCallbackWithErrorDescription(XMLFragment &fragment)
{
	int code;
	RETURN_VOID_IF_ERROR(TakeErrorCode(fragment, code));
	fragment.RestartFragment();
	if (fragment.EnterElement(UNI_L("Response")) && fragment.EnterElement(UNI_L("ErrorResponse")) && EnterFirstElement(fragment, UNI_L("errorDescription")))
	{
		TempBuffer buffer;
		RETURN_VOID_IF_ERROR(fragment.GetAllText(buffer));
		m_callback->SetErrorCodeAndDescription(code, buffer.GetStorage());
	}
}

BOOL JILNetworkRequestHandler::EnterFirstElement(XMLFragment &fragment, const XMLExpandedName &name)
{
	fragment.RestartCurrentElement();
	while (fragment.HasMoreElements())
	{
		fragment.EnterAnyElement();
		if (fragment.GetElementName() == name)
			return TRUE;
		fragment.LeaveElement();
	}
	return FALSE;
}

void JILNetworkRequestHandler::OnTimeOut(OpTimer* timer)
{
	++m_retryCount;
	SendRequest();
}


#endif // DAPI_JIL_NETWORKRESOURCES_SUPPORT
