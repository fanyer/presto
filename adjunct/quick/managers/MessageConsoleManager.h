/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */
#ifndef MESSAGE_CONSOLE_MANAGER_H
#define MESSAGE_CONSOLE_MANAGER_H

#ifdef OPERA_CONSOLE

#include "adjunct/quick/managers/DesktopManager.h"
#include "modules/console/opconsoleengine.h"
#include "modules/console/opconsolefilter.h"
#include "modules/console/opconsoleview.h"

class MailMessageDialog;
class MessageConsoleDialog;

#define g_message_console_manager (MessageConsoleManager::GetInstance())


/**
 * This class keeps track of the dialog part of the message console dialog. 
 * It manages when it needs to be created, and inserts messages into it.
 *
 * This is the class that the OpConsoleView sees when it posts messages.
 * It is the responsebility of this class to know if the visible dialog
 * exists and to create it when needed. The interface consists mainly of 
 * two functions apart from the constructor and destructor:
 *
 * PostNewMessage receives a pointer to a message that is to be displayed. 
 * The MessageConsoleHandler makes sure that the dialog exists, and then 
 * calls the corresponding function in the dialog class to post the 
 * message.
 *
 * ConsoleDialogClosed MUST be called by the dialog when it is deleted. If the 
 * dialog fails to notify the handler that it has died, incoming new 
 * messages will not correctly trigger the creation of a new dialog
 * before trying to insert the new message, which will lead to crashing
 * or at least no message will be displayed.
 */
class MessageConsoleManager :
		public DesktopManager<MessageConsoleManager>,
		public OpConsoleViewHandler
{
public:

	/**
	 * Constructor. You must also call Construct.
	 */
	MessageConsoleManager();

	/**
	 * Second phase constructor.
	 * Creates the view that channels messages to this console.
	 * @return OpStatus::ERR_NO_MEMORY if OOM.
	 */
	virtual OP_STATUS Init();

	/**
	 * Destructor. Deletes the view and makes sure that the dialog is closed.
	 */
	~MessageConsoleManager(); 

	/**
	 * The function that is used by the view to post new messages to the 
	 * console. This function creates a new dialog if it is closed, and 
	 * decides wheter to bring it to the front and give it focus.
	 *
	 * The message may contain severities from the OpConsoleEngine::Severity enum, which are
	 * Debugging, Verbose, Information, Error and Critical. The console
	 * maps the severities to three different severities visually: Information, 
	 * Warning and Error. Debugging and Verbose will be mapped to Information,
	 * Information will map to Warning, and Error and Critical will be
	 * mapped to Error. BUT: Messages with severity Critical will always cause 
	 * the dialog to be brought to focus. So if you have a warning message 
	 * that requires user attention, use severity critical. Also, the filter should
	 * allow messages with severity critical through even if that component is
	 * not selected.
	 *
	 * Messages from the mailer will get special treatment because this is the 
	 * most common type of error that have to be displayed.
	 *
	 * @param message The message to display. 
	 *
	 */
	OP_STATUS PostNewMessage(const OpConsoleEngine::Message* message);

	/**
	 * If you need to post more than one message at a time to the console
	 * you should use BeginUpdate before the first message is posted,
	 * and EndUpdate after the last one is updated to speed up posting.
	 */
	void BeginUpdate();
	void EndUpdate();

	/**
	 * Function that is used to open the dialog regardless of whether there are messages or not.
	 * Used when the user manually opens the console from the Opera UI.
	 *
	 * @return OK when the dialog is opened or was already open. OOM if the dialog can't be created.
	 */
	OP_STATUS OpenDialog();

	/**
	 * Funciton to open the dialog with a special component selected.
	 *
	 * Used when opening console to display mailer warnings.
	 *
	 * @param component The component to select.
	 * @param severity The severity to select.
	 * @return ERR if there was a problem, OK otherwise.
	 */
	OP_STATUS OpenDialog(OpConsoleEngine::Source component, OpConsoleEngine::Severity severity);

	/**
	 * Closes the console dialog.
	 * 
	 * Used to close the console dialog when needed
	 */
	void CloseDialog();

	/**
	 * Console Dialog closed. The console dialog must call this function when it closes.
	 * 
	 * This will enable the handler to null out the dialog pointer to avoid crashes and
	 * ensure that the dialog is displayed again, if a new message comes through.
	 */
	void ConsoleDialogClosed();

	/**
	 * Mail Message Dialog closed. Must be called by the mail message dialog when it closes.
	 *
	 * Enables the handler to know if it needs to open a new dialog or if it can post the 
	 * new message to the old dialog.
	 */
	void MailMessageDialogClosed();

	// Message console interaction management. Allows the user to
	// Change the filter from the message console while it is running.

	/**
	 * Call to obtain current filtering state.
	 *
	 * @param source Set with current source filter value.
	 * @param severity Set with current severity filter value.
	 */
	void GetSelectedComponentAndSeverity(OpConsoleEngine::Source& source, OpConsoleEngine::Severity& severity);

	/**
	 * Call when the user selected a specific component and severity.
	 * Updates the filter with the settings, but these settings will
	 * be dropped when the console is closed. Then the filter is reset
	 * to what the prefs say.
	 *
	 * @param source The source to set
	 * @param severity The severity to set.
	 */
	void SetSelectedComponentAndSeverity(OpConsoleEngine::Source source, OpConsoleEngine::Severity severity);

	/**
	 * Call when the user selected all components and a specific severity.
	 * Updates the filter with the settings, but these settings will
	 * be dropped when the console is closed. Then the filter is reset
	 * to what the prefs say.
	 *
	 * @param severity The severity to set.
	 */
	void SetSelectedAllComponentsAndSeverity(OpConsoleEngine::Severity severity);

	/**
	 * Call when the user cleared the filter.
	 * This sets a permanent cutoff point in the filter that will be persistent 
	 * even after the user closes the message console.
	 */
	void UserClearedConsole();

	/** 
	 * Turn M2 console loggin on or off.
	 *
	 * @param enable TRUE to turn M2 logging on, FALSE to turn off.
	 */
	void SetM2ConsoleLogging(BOOL enable);

	// Message console preferences management. (For JavaScript)
	/**
	 * Turn JavaScript errors on or off.
	 */
	void SetJavaScriptConsoleLogging(BOOL enable);

	/**
	 * Retrieve the JavaScript setting
	 */
	BOOL GetJavaScriptConsoleSetting();
	
	/**
	 * Turn JavaScript errors on or off for a specific site.
	 * The change only applies if the site setting is _different_
	 * from the previous setting, or the general setting. 
	 * This means that you don't get site settings written 
	 * unless you actually change the setting for a site.
	 */
	void SetJavaScriptConsoleLoggingSite(const OpString &site, BOOL enable);

	/**
	 * Retrieves site specific JavaScript console setting.
	 * If this site is not overridden it returns the general
	 * javascript console setting.
	 */
	BOOL GetJavaScriptConsoleSettingSite(const OpString &site);

private:
	// Members
	MessageConsoleDialog*		m_message_console_dialog; ///< The dialog that this instance keeps track of.
	MailMessageDialog*			m_mail_message_dialog;    ///< The mail message dialog that will open on mail errors.
	OpConsoleView*				m_using_view;             ///< The console view that this handler created to listen for messages.
	OpConsoleEngine::Source		m_last_source;        ///< The source filter setting stored per session.
	OpConsoleEngine::Severity	m_last_severity;       ///< The severity filter setting stored per session.

	/**
	 * This function ensures that the member m_message_console_dialog exists. If
	 * it doesen't it will instantiates a new MessageConsoleDialog on the heap using new, and calls Init on it.
	 * It will return OOM if new fails. 
	 * @param front_and_focus If true, the dialog is brought to front and focussed. Otherwise it is left to whatever state it is in.
	 */
	OP_STATUS MakeNewMessageConsole(BOOL front_and_focus);

	/**
	 * Function to read the current filter state from prefs.
	 * @param filter The filter to fill with information.
	 * @return OK if the read was ok, ERR otherwise.
	 */
	OP_STATUS ReadFilterFromPrefs(OpConsoleFilter &filter);

	/**
	 * Function to write a filter to prefs.
	 * @param filter The filter to write.
	 * @return OK if the write was successful, ERR otherwise.
	 */
	OP_STATUS WriteFilterToPrefs(const OpConsoleFilter &filter);
};

#endif // OPERA_CONSOLE
#endif // MESSAGE_CONSOLE_MANAGER_H
