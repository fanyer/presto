// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Espen Sand
//

#ifndef __KIOSK_MANAGER_H__
#define __KIOSK_MANAGER_H__

#include "modules/hardcore/timer/optimer.h"

class Dialog;
class OpInputAction;


class KioskManager : public OpTimerListener
{
private:
	KioskManager();
	~KioskManager();

	KioskManager(const KioskManager&);
	KioskManager& operator=(const KioskManager&);

public:
	static KioskManager *GetInstance();

	/**
	 * Parses the string flag and sets an internal flag.
	 *
	 * @param arg The command line value. Any flag specifiers must
	 *        not be a part of the string.
	 *
	 * @return returns TRUE if argument was a valid kiosk mode flag.
	 */
	int  ParseFlag( const char* arg, const char* next_arg = NULL );

	BOOL GetEnabled()            { Check(); return m_packed.kiosk_mode; }
	BOOL GetKioskButtons()       { Check(); return m_packed.kiosk_buttons; }
	BOOL GetKioskResetStation()  { Check(); return m_packed.kiosk_resetstation; }
	BOOL GetKioskWindows()       { Check(); return m_packed.kiosk_windows; }
	BOOL GetKioskNormalScreen()  { Check(); return m_packed.kiosk_normalscreen; }
	BOOL GetKioskSpeeddial()	 { Check(); return m_packed.kiosk_speeddial; }
	BOOL GetNoChangeButtons()    { Check(); return m_packed.no_change_buttons; }
	BOOL GetNoChangeFullScreen() { Check(); return m_packed.no_change_fullscreen;}
	BOOL GetNoContextMenu()      { Check(); return m_packed.no_contextmenu; }
	BOOL GetNoDownload()         { Check(); return m_packed.no_download; }
	BOOL GetNoExit()             { Check(); return m_packed.no_exit; }
	BOOL GetNoHotlist()          { Check(); return m_packed.no_hotlist; }
	BOOL GetNoKeys()             { Check(); return m_packed.no_keys; }
	BOOL GetNoMailLinks()        { Check(); return m_packed.no_maillinks; }
	BOOL GetNoMenu()             { Check(); return m_packed.no_menu; }
	BOOL GetNoPrint()            { Check(); return m_packed.no_print; }
	BOOL GetNoSave()             { Check(); return m_packed.no_save; }
	BOOL GetNoSplash()           { Check(); return m_packed.no_splash; }
	BOOL GetNoSysMenu()          { Check(); return m_packed.no_sysmenu; }
	BOOL GetResetOnExit()        { Check(); return m_packed.reset_on_exit; }

	static BOOL GetMailEnabled() { return m_mail_enabled; }

	void SetEnableFilter(BOOL enable) {m_packed.enable_filter=enable;}

	/**
	 * Updates activity flag. Should be called for each keyboard 
	 * and mouse event.
	 */
	void RegisterActivity();

	/**
	 * Test for inactivity and starts auto countdown if necessary
	 *
	 * @param timer The timer that triggered the timeout 
	 */
	void OnTimeOut(OpTimer* timer);

	/**
	 * Called when auto countdown dialog has been closed
	 */
	void OnDialogClosed() { m_dialog = 0; }

	/**
	 * Must be called when the kiosk mode should be automatically 
	 * reset becaise of inactivity
	 */
	void OnAutoReset();

	/**
	 * Must be called before the application exits and while the 
	 * message loop is in progress
	 */
	void OnExit();

	/**
	 * Tests actions and determines if it is allowed in the current 
	 * kiosk setup.
	 *
	 * @param action The action to test
	 *
	 * @return TRUE if the action was blocked (not allowed), 
	 *         otherwise FALSE
	 */
	BOOL ActionFilter(OpInputAction* action);

	/**
	 * Performs all steps required to clear private data in a kiosk mode setting
	 */
	void ClearPrivateData();

	/**
	 * Prints available command line flags
	 */
	static void PrintCommandlineOptions();

private:

	struct KioskSettings
	{
		KioskSettings()
			: kiosk_mode(0)
			, kiosk_buttons(0)
			, kiosk_resetstation(0)
			, kiosk_windows(0)
			, kiosk_normalscreen(0)
			, kiosk_speeddial(0)
			, no_change_buttons(0)
			, no_change_fullscreen(0)
			, no_contextmenu(0)
			, no_download(0)
			, no_exit(0)
			, no_hotlist(0)
			, no_keys(0)
			, no_maillinks(0)
			, no_menu(0)
			, no_print(0)
			, no_save(0)
			, no_splash(0)
			, no_sysmenu(0)
			, reset_on_exit(0)
		{}
		unsigned int kiosk_mode:1;
		unsigned int kiosk_buttons:1;
		unsigned int kiosk_resetstation:1;
		unsigned int kiosk_windows:1;
		unsigned int kiosk_normalscreen:1;
		unsigned int kiosk_speeddial:1;
		unsigned int no_change_buttons:1;
		unsigned int no_change_fullscreen:1;
		unsigned int no_contextmenu:1;
		unsigned int no_download:1;
		unsigned int no_exit:1;
		unsigned int no_hotlist:1;
		unsigned int no_keys:1;
		unsigned int no_maillinks:1;
		unsigned int no_menu:1;
		unsigned int no_print:1;
		unsigned int no_save:1;
		unsigned int no_splash:1;
		unsigned int no_sysmenu:1;
		unsigned int reset_on_exit:1;
		// internal flags
		unsigned int settings_checked:1;
		unsigned int mouse_or_key_activety:1;
		unsigned int enable_filter:1;
	};
	KioskSettings m_packed;

	INT32 m_ticks_since_last_activety;
	OpTimer* m_timer;
	Dialog* m_dialog;

	OpString8 m_useragent;

	// Must be accessed at an earily stage (when setting up m2) 
	static BOOL m_mail_enabled; 

private:
	void Check();
};




#endif
