/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Øyvind Østlund (oyvindo)
 *
 */

#include "core/pch.h"

#ifdef WEB_TURBO_MODE

#include "adjunct/quick/managers/OperaTurboManager.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/models/DesktopWindowCollection.h"
#include "adjunct/quick/windows/DocumentDesktopWindow.h"
#include "adjunct/quick_toolkit/widgets/OpProgressbar.h"
#include "adjunct/desktop_util/string/stringutils.h"

#ifdef NETWORK_STATISTICS_SUPPORT
#include "modules/url/protocols/comm.h"
#endif
#include "modules/history/OpHistoryModel.h"
#include "modules/history/OpHistoryPage.h"
#include "modules/history/OpHistorySiteFolder.h"
#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/prefs/prefsmanager/collections/pc_js.h"
#include "modules/url/url_man.h"

#define LOW_BANDWIDTH_LIMIT (16*1024) // bytes/s
#define HIGH_BANDWIDTH_LIMIT (64*1024) // bytes/s
#define LOW_DATA_LIMIT (1024*1024) // bytes
#define MIN_VISITED_PAGES 8

#define NETWORK_TEST_TIMEOUT (60*1000) // ms

/***********************************************************************************
 **
 **	~OperaTurboManager
 **
 ***********************************************************************************/
OperaTurboManager::OperaTurboManager()
: m_turbo_dialog(NULL),
  m_show_notification(FALSE),
  m_slow_network(MAYBE),
  m_compression_rate(0.0),
  m_compression_rates(0),
  m_bytes_saved(0),
  m_timer(NULL),
  m_notification_timer(NULL),
  m_timestamp(0),
  m_total_bytes(0),
  m_time_spendt(0),
  m_page_views(0)
{
}


/***********************************************************************************
 **
 **	~OperaTurboManager
 **
 ***********************************************************************************/
OperaTurboManager::~OperaTurboManager()
{
	if(m_turbo_dialog)
	{
		m_turbo_dialog->SetDialogListener(NULL);
	}
	OP_DELETE(m_timer);
	OP_DELETE(m_notification_timer);
}


/***********************************************************************************
 **
 **	Init
 **
 ***********************************************************************************/
OP_STATUS OperaTurboManager::Init()
{
	m_timer = OP_NEW(OpTimer, ());
	if(m_timer)
	{
		m_timestamp = g_timecache->CurrentTime();
		m_timer->Start(NETWORK_TEST_TIMEOUT);
		m_timer->SetTimerListener(this);
	}
	m_notification_timer = OP_NEW(OpTimer, ());
	if(m_notification_timer)
	{
		m_notification_timer->Start(1000);
		m_notification_timer->SetTimerListener(this);
	}
	
	int turbo_mode = g_pcui->GetIntegerPref(PrefsCollectionUI::OperaTurboMode);
	switch(turbo_mode)
	{
		case OperaTurboManager::OperaTurboAuto:
		{
			break;
		}
		case OperaTurboManager::OperaTurboOn:
		{
			// work around for DSK-281507, Crash in TransfersPanel::TransfersPanel
			// we don't want to show the page too early as other managers are not initialized yet
			// when OperaTurboMode is on the turbo info page probably has already been shown once
			// but not reflected in prefs for some reason.
			TRAPD(err, g_pcui->WriteIntegerL(PrefsCollectionUI::ShowOperaTurboInfo, FALSE));

			EnableOperaTurbo(true);
			break;
		}
		case OperaTurboManager::OperaTurboOff:
		{
			EnableOperaTurbo(false);
			break;
		}
	}

	g_windowCommanderManager->SetWebTurboUsageListener(this);

	RETURN_IF_ERROR(m_state_modifier.Init());
	return OpStatus::OK;
}


/***********************************************************************************
 **
 **	OnClose
 **
 ***********************************************************************************/
void OperaTurboManager::OnClose(Dialog* dialog)
{
	m_show_notification = FALSE;
	m_state_modifier.SetAttention(false);
	m_turbo_dialog = NULL;
}


/***********************************************************************************
 **
 **	OnTimeOut
 **
 ***********************************************************************************/
void OperaTurboManager::OnTimeOut(OpTimer* timer)
{
#ifdef NETWORK_STATISTICS_SUPPORT
	if(m_timer == timer)
	{
		INT32 nHistoryItems = 0;
		INT32 nItems = g_globalHistory->GetCount();
		
		OpVector<LexSplayKey> keys;
		
		for(INT32 i=0; i<nItems; i++)
		{
			HistoryPage *item = g_globalHistory->GetItemAtPosition(i);
			
			if(item)
			{
				time_t accessed = item->GetAccessed();
				
				if(accessed > m_timestamp)
				{
					if(item->GetSiteFolder())
					{
						LexSplayKey* key = (LexSplayKey*)item->GetSiteFolder()->GetKey();
						if(keys.Find(key) < 0)
						{
							nHistoryItems++;
							keys.Add(key);
						}
					}
				}
				else
				{
					break;
				}
			}
		}
		
		Network_Statistics_Manager* net_man = urlManager->GetNetworkStatisticsManager();
		if(net_man && !m_turbo_dialog)
		{
			int last_max_rate = net_man->getMaxTransferSpeedLastTenMinutes();
			//int total_max_rate = net_man->getTransferSpeedMaxTotal();
			//printf("Evaluate: rate = %d transferred = %d nopages = %d\n", last_max_rate, (int)(bytes_transferred-m_bytes_transferred), nHistoryItems);
			if(last_max_rate > 0)
			{
				if(net_man->getBytesTransferredLastTenMinutes() > LOW_DATA_LIMIT && nHistoryItems >= MIN_VISITED_PAGES)
				{
					EvaluateTransferRate(last_max_rate);
					m_timestamp = g_timecache->CurrentTime();
					if(m_slow_network == NO)
					{
						// Dont check again if we're on a fast network.
						return;
					}
				}
			}
		}
		m_timer->Start(NETWORK_TEST_TIMEOUT);
		return;
	}
#endif
	if(m_notification_timer == timer)
	{
		if(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseWebTurbo) && g_pcui->GetIntegerPref(PrefsCollectionUI::ShowNetworkSpeedNotification))
		{
			OpString str, button_text;
			g_languageManager->GetString(Str::S_OPERA_WEB_TURBO_ENABLED_NOTIFICATION_TEXT, str);

			m_state_modifier.OnShowNotification(str, button_text, NULL);
		}
		OP_DELETE(m_notification_timer);
		m_notification_timer = NULL;
	}
}


/***********************************************************************************
 **
 **	OnUsageChanged
 **
 ***********************************************************************************/
void OperaTurboManager::OnUsageChanged(TurboUsage status)
{
	if (status == NORMAL)
		m_state_modifier.SetAttention(false);
}


/***********************************************************************************
 **
 **	EvaluateTransferRate
 **
 ***********************************************************************************/
void OperaTurboManager::EvaluateTransferRate(double rate)
{
	int turbo_mode = g_pcui->GetIntegerPref(PrefsCollectionUI::OperaTurboMode);
	int using_web_turbo = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseWebTurbo);
	
	if(rate < LOW_BANDWIDTH_LIMIT)
	{
		m_slow_network = YES;
	}
	else if(rate > HIGH_BANDWIDTH_LIMIT)
	{
		m_slow_network = NO;
	}
	else
	{
		m_slow_network = MAYBE;
	}
	
	if(m_slow_network == YES && !using_web_turbo)
	{
		if(!m_show_notification)
			ShowNetworkSpeedNotification(TRUE);
		
		switch(turbo_mode)
		{
			case OperaTurboOn:
			case OperaTurboOff:
			{
				if(!m_show_notification)
				{
					m_show_notification = TRUE;
					m_state_modifier.SetAttention(true);
				}
				break;
			}
			case OperaTurboAuto:
			{
				EnableOperaTurbo(true);
				break;
			}
		}
		return;
	}
	else if(m_slow_network == NO && using_web_turbo)
	{
		if(!m_show_notification)
			ShowNetworkSpeedNotification(FALSE);
		
		switch(turbo_mode)
		{
			case OperaTurboOn:
			case OperaTurboOff:
			{
				if(!m_show_notification)
				{
					m_show_notification = TRUE;
					m_state_modifier.SetAttention(true);
				}
				break;
			}
			case OperaTurboAuto:
			{
				EnableOperaTurbo(false);
				break;
			}
		}
		return;
	}
	else if(m_show_notification)
	{
		m_show_notification = FALSE;
		m_state_modifier.SetAttention(false);
	}
}


/***********************************************************************************
 **
 **	AddCompressionRate
 **
 ***********************************************************************************/
void OperaTurboManager::AddCompressionRate(INT32 turboTransferredBytes, INT32 turboOriginalTransferredBytes, INT32 prev_turboTransferredBytes, INT32 prev_turboOriginalTransferredBytes)
{
	if(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseWebTurbo))
	{
		m_bytes_saved -= prev_turboOriginalTransferredBytes - prev_turboTransferredBytes;
		m_bytes_saved += turboOriginalTransferredBytes - turboTransferredBytes;
		float rate = (float)turboOriginalTransferredBytes/(float)turboTransferredBytes;
		m_compression_rate = rate/(m_compression_rates+1) + m_compression_rate*m_compression_rates/(m_compression_rates+1);
		m_compression_rates++;
	}
}

/***********************************************************************************
 **
 **	AddToTotalBytes
 **
 ***********************************************************************************/
void OperaTurboManager::AddTototalBytes(INT32 bytes, double time_spendt)
{
    m_total_bytes += bytes;
    m_time_spendt += time_spendt;
}

/***********************************************************************************
 **
 **	GetAverageSpeed
 **
 ***********************************************************************************/
INT32 OperaTurboManager::GetAverageSpeed()
{
    if (m_time_spendt && !op_isnan(m_time_spendt) && !op_isinf(m_time_spendt))
		return m_total_bytes / (m_time_spendt / 1000);
		
	return 0;
}


/***********************************************************************************
 **
 **	ResetBytesCounter
 **
 ***********************************************************************************/
void OperaTurboManager::ResetUsageReportCounters()
{
    m_total_bytes = 0;
    m_time_spendt = 0.0;
    m_page_views = 0;
}

/***********************************************************************************
 **
 **	GiveTotalPageViews
 **   - Total success full page loads with Opera Turbo
 **     After the function is called, the counter is reset
 **
 ***********************************************************************************/
INT32 OperaTurboManager::GiveTotalPageViews()
{
    INT32 ret = m_page_views;
    m_page_views = 0;
    
    return ret;
}

/***********************************************************************************
 **
 **	AddToToalpageViews
 **   - Add 'pages' more views to the current page view count
 **
 ***********************************************************************************/
void OperaTurboManager::AddToTotalPageViews(UINT32 pages)
{
	m_page_views += pages;
}

/***********************************************************************************
 **
 **	GetShowNotification
 **
 ***********************************************************************************/
BOOL OperaTurboManager::GetShowNotification() const
{
	return m_show_notification && g_pcui->GetIntegerPref(PrefsCollectionUI::ShowNetworkSpeedNotification);
}


/***********************************************************************************
 **
 **	ResetCounters
 **
 ***********************************************************************************/
void OperaTurboManager::ResetCounters()
{
	m_compression_rate = 0.0;
	m_compression_rates = 0;
	m_bytes_saved = 0;
}


/***********************************************************************************
 **
 **	EnableOperaTurbo
 **
 ***********************************************************************************/
bool OperaTurboManager::EnableOperaTurbo(bool enable)
{
	OperaTurboMode current_mode = GetTurboMode();

	if(( enable && current_mode == OperaTurboManager::OperaTurboOn) ||
	   (!enable && current_mode == OperaTurboManager::OperaTurboOff))
		return false;

	TRAPD(err, g_pcnet->WriteIntegerL(PrefsCollectionNetwork::UseWebTurbo, enable));

	if(enable)
	{
		TRAP(err, g_pcui->WriteIntegerL(PrefsCollectionUI::OperaTurboMode, OperaTurboOn));
		m_state_modifier.SetWidgetStateToEnabled();
	}
	else
	{
		TRAP(err, g_pcui->WriteIntegerL(PrefsCollectionUI::OperaTurboMode, OperaTurboOff));
		m_state_modifier.SetWidgetStateToDisabled();
	}

	// Change turbo mode in all pages
	OpVector<DesktopWindow> documents;
	OpStatus::Ignore(g_application->GetDesktopWindowCollection().GetDesktopWindows(OpTypedObject::WINDOW_TYPE_DOCUMENT, documents));

	for(UINT32 i = 0; i < documents.GetCount(); i++)
	{
		DocumentDesktopWindow* document = static_cast<DocumentDesktopWindow*>(documents.Get(i));
		if(document->GetWindowCommander() && !document->GetWindowCommander()->GetPrivacyMode())
			document->GetWindowCommander()->SetTurboMode(enable);
	}

	if(enable && g_pcui->GetIntegerPref(PrefsCollectionUI::ShowOperaTurboInfo))
	{
		g_application->GoToPage(UNI_L("http://redir.opera.com/turbo/"), TRUE);
		TRAPD(err, g_pcui->WriteIntegerL(PrefsCollectionUI::ShowOperaTurboInfo, FALSE));
	}

	return true;
}


/***********************************************************************************
 **
 **	SetOperaTurboMode
 **
 ***********************************************************************************/
void OperaTurboManager::SetOperaTurboMode(int turbo_mode)
{
	switch(turbo_mode)
	{
		case OperaTurboAuto:
		{
			TRAPD(err, g_pcui->WriteIntegerL(PrefsCollectionUI::OperaTurboMode, turbo_mode));
			m_state_modifier.SetWidgetStateToEnabled();
			break;
		}
		case OperaTurboOn:
		{
			EnableOperaTurbo(true);
			break;
		}
		case OperaTurboOff:
		{
			EnableOperaTurbo(false);
			break;
		}
	}
}

OperaTurboManager::OperaTurboMode OperaTurboManager::GetTurboMode()
{
	int turbo_mode = g_pcui->GetIntegerPref(PrefsCollectionUI::OperaTurboMode);
	
	switch(turbo_mode)
	{
		case OperaTurboAuto:
			return OperaTurboAuto;
		case OperaTurboOn:
			return OperaTurboOn;
		default:
			return OperaTurboOff;
	}
}

/***********************************************************************************
 **
 **	ShowNetworkSpeedNotification
 **
 ***********************************************************************************/
void OperaTurboManager::ShowNetworkSpeedNotification(bool slow_net)
{
	if(g_pcui->GetIntegerPref(PrefsCollectionUI::ShowNetworkSpeedNotification))
	{
		int turbo_mode = g_pcui->GetIntegerPref(PrefsCollectionUI::OperaTurboMode);
		OpString str;
		OpString button_text;
		OpInputAction* action = NULL;
		switch(turbo_mode)
		{
			case OperaTurboAuto:
			{
				if(slow_net)
				{
					g_languageManager->GetString(Str::D_OPERA_TURBO_NOTIFICATION_SLOW_NET_AUTO, str);
				}
				else
				{
					g_languageManager->GetString(Str::D_OPERA_TURBO_NOTIFICATION_FAST_NET_AUTO, str);
				}					
				break;
			}

			case OperaTurboOff:
				if (slow_net)
				{	// DSK-359140
					int notifications_left = g_pcui->GetIntegerPref(PrefsCollectionUI::TurboSNNotificationLeft);
					if (notifications_left == 0)
							return;

					if (notifications_left > 0)
					{
						TRAPD(err, g_pcui->WriteIntegerL(PrefsCollectionUI::TurboSNNotificationLeft, notifications_left-1));
					}
				}
				//fall trough

			case OperaTurboOn:
			{
				action = OP_NEW(OpInputAction, (OpInputAction::ACTION_OPEN_OPERA_WEB_TURBO_DIALOG));
				g_languageManager->GetString(Str::D_OPERA_TURBO_NOTIFICATION_OPTIONS, button_text);
				if (slow_net)
				{
					g_languageManager->GetString(Str::D_OPERA_TURBO_NOTIFICATION_SLOW_NET, str);
				}
				else
				{
					g_languageManager->GetString(Str::D_OPERA_TURBO_NOTIFICATION_FAST_NET, str);
				}
				break;
			}
		}
		if(str.HasContent())
		{
			m_state_modifier.OnShowNotification(str, button_text, action);
		}
	}
}


/***********************************************************************************
 **
 **	ShowNotificationDialog
 **
 ***********************************************************************************/
void OperaTurboManager::ShowNotificationDialog(DesktopWindow* parent)
{
	if(!m_turbo_dialog)
	{
		m_turbo_dialog = OP_NEW(OperaTurboDialog, (m_slow_network));
		if(m_turbo_dialog)
		{
			m_turbo_dialog->Init(parent/*g_application->GetActiveDesktopWindow()*/);
			m_turbo_dialog->SetDialogListener(this);
		}
	}
}

#endif // WEB_TURBO_MODE
