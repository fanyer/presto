/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 */

#ifdef WEB_TURBO_MODE

#ifndef OPERA_TURBO_MANAGER_H
#define OPERA_TURBO_MANAGER_H

#include "adjunct/desktop_pi/desktop_notifier.h"
#include "adjunct/quick/dialogs/OperaTurboDialog.h"
#include "adjunct/quick/managers/DesktopManager.h"
#include "adjunct/quick/turbo/OperaTurboWidgetStateModifier.h"

#include "modules/windowcommander/OpWindowCommanderManager.h"


/***********************************************************************************
 *
 *  @class OperaTurboManager
 *
 *	@brief Mangager for Opera Turbo
 *
 ***********************************************************************************/
class OperaTurboManager
	: public DesktopManager<OperaTurboManager>,
	  public DialogListener,
	  public OpTimerListener,
	  public OpWebTurboUsageListener
{
public:
	OperaTurboManager();
	virtual ~OperaTurboManager();

	OP_STATUS Init();

	// DialogListener implemetation
	virtual void OnClose(Dialog* dialog);
	
	// OpTimerListener implementation
	virtual void OnTimeOut(OpTimer* timer);
	
	// OpWebTurboUsageListener implementation
	virtual void OnUsageChanged(TurboUsage status);

	void AddCompressionRate(INT32 turboTransferredBytes, INT32 turboOriginalTransferredBytes, INT32 prev_turboTransferredBytes, INT32 prev_turboOriginalTransferredBytes);
	
	// NB: Counts the total bytes when turbo is not enabled
    void AddTototalBytes(INT32 bytes, double time_spendt);
    
	// NB: Average speed of connection without turbo enabled
	INT32 GetAverageSpeed();
    
	// NB: Resets the temp values used for Usage Report
    void ResetUsageReportCounters();
    
    // Total success full page loads with Opera Turbo
    // The count will be given to the caller, and this counter is reset.
    INT32 GiveTotalPageViews();
    
    // Add 'pages' more views to the current page view count
    void  AddToTotalPageViews(UINT32 pages);

	BOOL GetShowNotification() const;
	void ShowNotificationDialog(DesktopWindow* parent);

	class OperaTurboManagerListener
	{
	public:
		virtual ~OperaTurboManagerListener() {}
		virtual void OnChange() = 0;
		virtual void OnShowNotification(const OpString& text, const OpString& button_text, OpInputAction* action = NULL) = 0;
	};

	enum OperaTurboMode
	{
		OperaTurboAuto,
		OperaTurboOn,
		OperaTurboOff
	};

	float GetCompressionRate() const { return m_compression_rate; }
	int GetBytesSaved() const { return m_bytes_saved; }

	void ResetCounters();
	
	bool EnableOperaTurbo(bool enable);
	void SetOperaTurboMode(int turbo_mode);
	OperaTurboMode GetTurboMode();

	OperaTurboWidgetStateModifier*		GetWidgetStateModifier()	{ return &m_state_modifier; }

private:
	void ShowNetworkSpeedNotification(bool slow_net);
	void EvaluateTransferRate(double rate);
	
	OperaTurboDialog*						m_turbo_dialog;
	BOOL									m_show_notification;
	BOOL3									m_slow_network;
	float									m_compression_rate;
	int										m_compression_rates;
	int										m_bytes_saved;
	OpTimer*								m_timer;
	OpTimer*								m_notification_timer;
	time_t									m_timestamp;
	INT32                                   m_total_bytes;
	double                                  m_time_spendt;
	UINT32                                  m_page_views;
	OperaTurboWidgetStateModifier           m_state_modifier;
};

#define g_opera_turbo_manager (OperaTurboManager::GetInstance())

#endif // OPERA_TURBO_MANAGER_H

#endif // WEB_TURBO_MODE
