#include "core/pch.h"

//Core header files
#include "modules/widgets/OpWidget.h"

//Platform header files
#include "adjunct/quick/managers/TipsManager.h"

//Global objects and constant definations

TipsManager::TipsManager()
	: m_tips_token(FALSE)
{

}

TipsManager::~TipsManager()
{
	TipsConfigManager::Destroy(m_tips_config_mgr);
}

OP_STATUS	
TipsManager::Init()
{
	RETURN_OOM_IF_NULL(m_tips_config_mgr = TipsConfigManager::GetInstance());
	OpStatus::Ignore(m_tips_config_mgr->LoadConfiguration());

	m_timer_tips_token.SetTimerListener(this);

	return OpStatus::OK;
}

OP_STATUS		
TipsManager::GetTipsConfigData(const OpStringC16& tip_name, TipsConfigGeneralInfo &tip_info)
{
	return m_tips_config_mgr->GetTipsConfigData(tip_name, tip_info);
}

BOOL
TipsManager::CanTipBeDisplayed(const OpStringC16& tip_name, TipsConfigGeneralInfo &tip_info)
{
	if (m_tips_config_mgr->IsTipsNeverOff())
	{
		OP_STATUS status = GetTipsConfigData(tip_name , tip_info);
		return OpStatus::IsSuccess(status) ? TRUE : FALSE;
	}

	if (!IsTipsSystemEnabled())
		return FALSE;

	if (m_tips_token)	//If m_tips_token is not FALSE, it means someone has displayed a tip.
		return FALSE;

	if (OpStatus::IsError(GetTipsConfigData(tip_name , tip_info)))
		return FALSE;

	if (!tip_info.CanTipBeDisplayed())
		return FALSE;

	return TRUE;
}

OP_STATUS
TipsManager::GrabTipsToken(TipsConfigGeneralInfo &tip_info)
{
	if (m_tips_config_mgr->IsTipsNeverOff())
		return OpStatus::OK;

	if (m_tips_token == TRUE)	//If m_tips_token is not FALSE, it means someone has displayed a tip.
		return OpStatus::ERR;
	
	RETURN_IF_ERROR(MarkTipShown(tip_info));
	
	m_tips_token = TRUE;

	m_timer_tips_token.Start(m_tips_config_mgr->GetTipsDisplayInterval());

	return OpStatus::OK;
}

OP_STATUS
TipsManager::MarkTipShown(TipsConfigGeneralInfo &tip_info)
{
	return m_tips_config_mgr->UpdateTipsLastDisplayedInfo(tip_info);
}

void
TipsManager::OnTimeOut(OpTimer* timer)
{
	if (timer == &m_timer_tips_token)
	{
		m_tips_token = FALSE;
	}
}