/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 * @file MessageConsoleDialog.h
 *
 * This file contains the class is the Message console dialog that pops 
 * up when Opera needs to convey a message to the user. (For instance to 
 * notify of JavaScript errors.)
 *
 * In addition we have the controlling class MessageConsoleHandler that 
 * keeps track of the dialog.
 */
#ifndef MESSAGE_CONSOLE_DIALOG
#define MESSAGE_CONSOLE_DIALOG

#ifdef OPERA_CONSOLE

#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "modules/console/opconsoleengine.h"
#include "adjunct/desktop_util/treemodel/simpletreemodel.h"

class MessageConsoleManager;


/**
 * This dialog contains the user interface part of the message console.
 *
 * The console dialog is conrolled by a MessageConsoleManager and the opening 
 * and closing of this dialog should go through that class.
 *
 * The public interface of the dialog is made up from the constructor and the
 * PostConsoleMessage-function.
 *
 * Constructing the class requires an existing OpConsoleView where the dialog 
 * sends new filters when the user changes the settings. In addition a 
 * pointer to the handler instance that owns the dialog is used. This is 
 * required so the console dialog can inform the handler when it is being 
 * deleted after the user has selected to close it.
 *
 * The PostConsoleMessage only requires the message that is to be posted. 
 * little or no checking is done to see if the message is actually in the 
 * set selected by the user. It is the callers responsibility to respect the 
 * filter that has been set by the user and not pass messages that are not to 
 * be displayed.
 *
 * No calls should be made to PostConsoleMessage by classes other than
 * MessageConsoleManager. This handler is used as a gateway to the dialog
 * so that the dialog can be created and deleted randomly without the
 * core console module having to keep track of it. The handler is supposed to
 * know whether the dialog exists or not, and create it whenever needed. 
 *
 */
class MessageConsoleDialog : public Dialog
{
public:
	/** Constructor */
	explicit MessageConsoleDialog(MessageConsoleManager& manager);
	/** Destructor */
	~MessageConsoleDialog();

	// Overloaded functions:
	Type				GetType()				{return DIALOG_TYPE_MESSAGE_CONSOLE_DIALOG;}
	const char*			GetWindowName()			{return "Message Console Dialog";}
	DialogType			GetDialogType()			{return TYPE_YES_NO;}
	BOOL				GetModality()			{return FALSE;}
	BOOL				GetIsConsole() 			{return TRUE;}
	INT32				GetButtonCount()		{return 2;};
	void				GetButtonInfo(INT32 index, OpInputAction*& action, OpString& text, BOOL& is_enabled, BOOL& is_default, OpString8& name);
	void				Clear();

	void				OnChange(OpWidget* widget, BOOL changed_by_mouse);
	void				OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks);
	virtual BOOL		OnInputAction(OpInputAction* action);
	void				OnInit();

	/**
	 * Function that can be used by the handler to 
	 * manipulate the settings the dropdowns in the dialog.
	 *
	 * NOTE: This function will clear the treeview and 
	 * call for all messages to be resent. Do not call a 
	 * resending function after calling this function, that
	 * will lead to duplication of messages.
	 *
	 * @param source The source to set.
	 * @param severity The severity to set.
	 *
	 * @return OK if ok, ERR if some problem.
	 */
	OP_STATUS			SelectSpecificSourceAndSeverity(OpConsoleEngine::Source source, OpConsoleEngine::Severity severity);

	/** 
	 * Function that offers the message engine to post messages to the console.
	 *
	 * @param comp  The component that posts the message.
	 * @param sev   The severity of the message.
	 * @param title The title that is to be displayed in the console.
	 * @param text  The text of the message.
	 *
	 * @return Ok if the message was posted correctly, error otherwise.
	 */
	OP_STATUS			PostConsoleMessage(const OpConsoleEngine::Message* message);

	/**
	 * If you need to post more than one message at a time to the console
	 * you should use BeginUpdate before the first message is posted,
	 * and EndUpdate after the last one is updated to speed up posting.
	 */
	void				BeginUpdate();
	void				EndUpdate();

private:

	/**
	 * Struct to hold url and window ID of lines that are hooked up
	 * to the source viewer.
	 */
	struct LookupElem
	{
		OpString		url_lookup; ///< The url where the source is.
		unsigned int	message_id; ///< The message id
		BOOL			message_id_set; ///< Set when using the message id
		int				window_id;  ///< The id of the window where the message originated from.
	};

	OpAutoVector<LookupElem>				m_lookup_list;           ///< This list is used to keep track of the windows that the urls belong to.
	SimpleTreeModel*						m_message_tree_model;    ///< The tree model that holds the messages.
	MessageConsoleManager* m_manager;               ///< The manager that created this dialog and that need notification when the dialog is closed.

	/**
	 * Function used to map strings to components.
	 *
	 * See the static message_string_mapping.
	 *
	 * @param string The string that needs lookup.
	 * @param component The component has the string 
	 * supplied in the string arg. This param will be 
	 * set by the function.
	 *
	 * @return OK if the string was found in either 
	 * the hardcoded strings or in the translations.
	 */
	OP_STATUS GetComponentFromString(const OpStringC &string, OpConsoleEngine::Source &component);
	/**
	 * Function used to map components to strings.
	 *
	 * See the static message_string_mapping.
	 *
	 * @param component The component to lookup.
	 * @param string The string which the function fills 
	 * with the text corresponding to the component.
	 *
	 * @return OK if the component is found, ERR otherwise.
	 */
	OP_STATUS GetStringFromComponent(const OpConsoleEngine::Source component, OpString &string);
	/** 
	 * Function used to store the message id in lines of the console that 
	 * reference an Opera Account error
	 * @param message_id The message id to store
	 * @param the position in the array where the message id needs to be stored.
	 * @return OK if the storing went well, ERR otherwise.
	 */
	OP_STATUS StoreMessageIdInLookupVector(unsigned int message_id, UINT32 pos);
	/** 
	 * Function used to store the url in lines of the 
	 * console that reference a url.
	 * @param url The url to store
	 * @param the position in the array where the url needs to be stored.
	 * @return OK if the storing went well, ERR otherwise.
	 */
	OP_STATUS StoreURLInLookupVector(const OpStringC &url, UINT32 window_id, UINT32 pos);
#if defined HAVE_DISK && !defined NO_EXTERNAL_APPLICATIONS
	/**
	 * Open the URL in the source viewer application.
	 *
	 * @param url Specific URL to view source for, if different from
	 *            message's.
	 * @return Status of the operation.
	 */
	OP_STATUS ShowURLInSourceViewer(const uni_char *urlname, UINT32 window_id);
#endif // HAVE_DISK && !NO_EXTERNAL_APPLICATIONS
	/**
	 * Function that fills the component dropdown. Call it once 
	 * when initializing the dialog.
	 *
	 * @return ERR if the dropdown was not found, OK otherwise.
	 */
	OP_STATUS FillComponentDropdown();

	/**
	 * Function that updates selected item in source and severity dropdowns
	 * based on passed in values.
	 *
	 * @param source The current state of source value.
	 * @param severity The current state of severity value.
	 *
	 * @return ERR if one or boths dropdowns were not found, OK otherwise.
	 */
	OP_STATUS UpdateDropdownsSelectedState(OpConsoleEngine::Source source, OpConsoleEngine::Severity severity);
};

/**
 * Message box used to inform the user of mailer errors. This is because 
 * the mailer channels messages to the console, but the console is too 
 * intimidating and useless to normal users to be good for that purpose.
 *
 * Using this messagebox gives the mailer errors a less 
 * frightening appearance.
 *
 * The message box knows about the console handler and can call its
 * functions to display the console should the user want to do that.
 */
class MailMessageDialog : public Dialog
{
public:
	MailMessageDialog(MessageConsoleManager& manager, const OpString& message);
	~MailMessageDialog();

	void				OnInit();
	void				OnClose(BOOL user_initiated);
	void				SetMessage(const OpString &message);

	Type				GetType()				{return DIALOG_TYPE_MAIL_MESSAGE;}
	const char*			GetWindowName()			{return "Mail Message Dialog";}
	DialogImage			GetDialogImageByEnum()	{return IMAGE_WARNING;}
	DialogType			GetDialogType()			{return TYPE_CLOSE;}
	BOOL				GetDoNotShowAgain()		{return TRUE;}
	BOOL				OnInputAction(OpInputAction* action);
private:
	MessageConsoleManager* m_manager;
	OpString			m_message;
};

#endif // OPERA_CONSOLE
#endif // MESSAGE_CONSOLE_DIALOG
