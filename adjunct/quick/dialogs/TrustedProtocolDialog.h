// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
//
// Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Espen Sand
//

#ifndef __TRUSTED_PROTOCOL_DIALOG_H__
#define __TRUSTED_PROTOCOL_DIALOG_H__

#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "modules/util/adt/opvector.h"
#include "modules/prefs/prefsmanager/prefstypes.h"

class TrustedProtocolEntry
{
public:
	enum EntryType
	{
		SOURCE_VIEWER_ENTRY,
		MAILTO_ENTRY,
		NNTP_ENTRY,
		OTHER_ENTRY
	};

	TrustedProtocolEntry()
	  : m_in_terminal(FALSE)
	  , m_can_delete(FALSE)
	  , m_is_protocol(FALSE)
	  , m_opera_native_support(FALSE)
	  , m_web_handler_support(FALSE)
	  , m_viewer_mode(UseCustomApplication)
	  , m_entry_type(OTHER_ENTRY)
	  , m_webmail_id(0)
	{}

	/**
	 * Initializes element with settings that can not be changed
	 */
	OP_STATUS Init(OpStringC protocol, OpStringC web_handler, OpStringC description, BOOL is_protocol);

	/**
	 * Initializes element with settings that can be modified by user
	 */
	OP_STATUS Set(OpStringC filename, ViewerMode mode, BOOL in_terminal, UINT32 webmail_id);

	/**
	 * Returns TRUE if entry holds a news protocol
	 */
	BOOL IsNewsProtocol()   const { return !m_protocol.Compare(UNI_L("news")) || !m_protocol.Compare(UNI_L("nntp")); }

	/**
	 * Returns TRUE if entry can be delected by the user
	 */
	BOOL CanDelete()        const { return m_can_delete; }

	/**
	 * Returns TRUE if entry holds a protocol. An item be a special entry (like 'view source') which is
	 * not a protocol
	 */
	BOOL IsProtocol()       const { return m_is_protocol; }

	/**
	 * Returns TRUE if entry can be handeled by Opera itself
	 */
	BOOL HasNativeSupport() const { return m_opera_native_support; }

	/**
	 * Returns TRUE if entry species that a custom application should be executed in a terminal
	 */
	BOOL InTerminal()       const { return m_in_terminal; }

	/**
	 * Returns TRUE if the assigned web handler is not allowed to be used.
	 * A web handler can be blocked by dynamic updates in Opera after it has been
	 * registered.
	 *
	 * @param id Used if entry is handling a 'mailto:' action. Then the id refers one of more web handlers
	 */
	BOOL IsWebHandlerBlocked(UINT32 id);

	/**
	 * Test and revert the execute action to UseInternalApplication (open with opera)
	 * if the action is set to UseWebService and the web service is blocked by the
	 * security manager.
	 *
	 * @return TRUE if the action was changed, otherwise FALSE
	 */
	BOOL ResetBlockedWebHandler();

	/**
	 * Returns TRUE if a web handler is associated with the protocol
	 */
	BOOL HasWebHandler();

	/**
	 * Returns active application (local or web based) for entry. The string is
	 * for display purposes only. It does not contain a path or url
	 *
	 * @param application On return the name
	 * @param is_formatted If set to non NULL the returned name can be formatted
	 *        if neccesary. @ref is_formatted' will then be TRUE
	 *
	 * @return OpStatus::OK on success, othwerwise OpStatus:ERR_NO_MEMORY
	 */
	OP_STATUS GetActiveApplication(OpString& application, BOOL* is_formatted=0);

	/**
	 * Returns active web service (application) for entry. The string is
	 * for display purposes only. It does not contain url
	 *
	 * @param web_service_name On return the name
	 * @param is_formatted If set to non NULL the returned name can be formatted
	 *        if neccesary. @ref is_formatted' will then be TRUE
	 *
	 * @return OpStatus::OK on success, othwerwise OpStatus:ERR_NO_MEMORY
	 */
	OP_STATUS GetWebServiceName(OpString& web_service_name, BOOL* is_formatted=0);

	/**
	 * Returns text that can be used in UI. It can be a translated string The string 
	 * is for display purposes only.
	 *
	 * @return The text
	 */
	const OpStringC& GetDisplayText() const { return m_displayed_text.HasContent() ? m_displayed_text : m_protocol; }

	 /**
	 * Returns default application
	 *
	 * @return The application
	 */
	const OpStringC& GetDefaultApplication();

	/**
	 * Returns custom application (entered by user)
	 *
	 * @return The application
	 */
	const OpStringC& GetCustomApplication() const { return m_custom_application; }

	/**
	 * Returns parameters for custom application
	 *
	 * @return The parameters
	 */
	const OpStringC& GetCustomParameters() const { return m_custom_parameters; }

	/**
	 * Returns m_protocol name. Will be set to a special name for SOURCE_VIEWER_ENTRY type
	 *
	 * @return The name
	 */
	const OpStringC& GetProtocol() const { return m_protocol; }

	/**
	 * Returns web handler name
	 *
	 * @return The web handler name
	 */
	const OpStringC& GetWebHandler() const { return m_web_handler; }

	/**
	 * Return the full path of executable with parameters for
	 * the custom application if any.
	 *
	 * @param command On return, the command. It be empty on a successful return
 	 *
	 * @return OpStatus::OK on success, othwerwise OpStatus:ERR_NO_MEMORY
	 */
	OP_STATUS  GetCommand(OpString & command) const;

	/**
	 * Return the entry type. See @ref EntryType
	 *
	 * @return the type
	 */
	EntryType  GetEntryType()   const { return m_entry_type; }

	/**
	 * Return the viewer mode. This is the mode Opera will use
	 * when dealing with a protocol or content type that matches
	 * this entry
	 *
	 * @return The mode
	 */
	ViewerMode GetViewerMode()  const { return m_viewer_mode; }

	/**
	 * Return the web mail identifier. Only valid if @ref GetEntryType()
	 * returns MAILTO_ENTRY
	 *
	 * @return The identifier
	 */
	UINT32   GetWebmailId()   const { return m_webmail_id; }

	/**
	 * Build a string that contains the handler name and parameters. The handler will be quoted
	 * to allow for handler with spaces in the name
	 *
	 * @param command On return, the prepared string
	 * @param handler Handler (executable) name.
	 * @param parameters Optional parameters to handler
	 *
	 * @return OpStatus::OK on success, othwerwise OpStatus:ERR_NO_MEMORY
	 */
	OP_STATUS JoinCommand(OpString & command, const OpStringC & handler, const OpStringC & parameters);

	/**
	 * Set text that can will be used for UI purposes.
	 *
	 * @param displayed_text Text to display
	 *
	 * @return OpStatus::OK on success, othwerwise OpStatus:ERR_NO_MEMORY
	 */
	OP_STATUS SetDisplayedText(const OpStringC& displayed_text) { return m_displayed_text.Set(displayed_text); }

	/**
	 * Set default application for protocol / content type
	 *
	 * @param default_application The application
	 *
	 * @return OpStatus::OK on success, othwerwise OpStatus:ERR_NO_MEMORY
	 */
	OP_STATUS SetDefaultApplication(const OpString& default_application) { return m_default_application.Set(default_application); }

	/**
	 * Sets if this entry can be delected by the user (through UI)
	 *
	 * @param value TRUE if deletion is possible
	 */
	void SetCanDelete(BOOL value) {m_can_delete = value;}

	/**
	 * Sets if this entry had a protocol or content type with native platform handler support
	 *
	 * @param value TRUE if support is present
	 */
	void SetHasNativeSupport(BOOL value) {m_opera_native_support = value;}

	/**
	 * Sets entry type. We support a set of types that need special handling
	 *
	 * @param type See @ref EntryType
	 */
	void SetEntryType(EntryType type) { m_entry_type = type; }

	/**
	 * Sets web mail identifier. Will only be used if entry has a type of MAILTO_ENTRY
	 *
	 * @param id The identifier
	 */
	void SetWebmailId(UINT32 webmail_id) { m_webmail_id = webmail_id; }

private:
	OP_STATUS SetProtocol(OpStringC protocol) { return m_protocol.Set(protocol); }
	OP_STATUS SetDescription(OpStringC description) { return m_description.Set(description); }
	OP_STATUS SetWebHandler(OpStringC web_handler) { return m_web_handler.Set(web_handler); }
	OP_STATUS SetCustomApplication(const OpString& handler) { return m_custom_application.Set(handler); }
	OP_STATUS SetCustomParameters(const OpVector<OpString> & parameters);
	void SetInTerminal(BOOL value) { m_in_terminal = value; }
	void SetIsProtocol(BOOL value) { m_is_protocol = value; }
	void SetViewerMode(ViewerMode mode) { m_viewer_mode = mode; }
	OP_STATUS SplitCommand(const OpStringC & command, OpString & handler, OpVector<OpString> & parameters);

private:
	OpString m_protocol;             // Protocol name (if any)
	OpString m_description;          // Description read from core item
	OpString m_displayed_text;       // String to be used UI. It can be a translated string
	OpString m_default_application;  // Default application for protocol
	OpString m_web_handler;          // Web handler for for protocol
	OpString m_custom_application;   // Custom (entered by user) application for for protocol
	OpString m_custom_parameters;    // Parameters for custom application
	BOOL m_in_terminal;              // Custom application shall run in a terminal
	BOOL m_can_delete;               // This item can be deleted by user if TRUE
	BOOL m_is_protocol;              // TRUE if protocol, otherwise an internal type (like 'view source')
	BOOL m_opera_native_support;     // TRUE if protocol has native support in opera
	BOOL m_web_handler_support;      // TRUE if a web handler is registred for this protocol
	ViewerMode m_viewer_mode;        // The viewer action core will use when executing this prototol
	EntryType m_entry_type;          // See EntryType There is special suepport for SOURCE_VIEWER, MAILTO and NNTP
	UINT32 m_webmail_id;             // Web mail handler id, only valid for SOURCE_VIEWER_ENTRY type
};


class TrustedProtocolList
{
public:
	enum ListContent
	{
		PROTOCOLS    = 1,
		APPLICATIONS = 2
	};

public:
	TrustedProtocolList() {}

	OP_STATUS Read(ListContent content);
	OP_STATUS Save(ListContent content) const;

	/**
	 * Test all known trusted protocol entries and revert the
	 * execute action to UseInternalApplication (open with opera)
	 * if the action is set to UseWebService and the web service
	 * is blocked by the security manager.
	 *
	 * @return OpStatus::OK on success, otherwise OpStatus::ERR_NO_MEMORY
	 */
	OP_STATUS ResetBlockedWebHandlers();

	void Delete(INT32 index);
	BOOL CanDelete(INT32 index) const;

	TrustedProtocolEntry* Add();
	TrustedProtocolEntry* Get(INT32 index) const;
	TrustedProtocolEntry* GetByString(const OpStringC& protocol) const;
	UINT32 GetCount() const { return m_list.GetCount(); }

	static OP_STATUS GetSourceViewer(OpString &handler);

private:
	OP_STATUS MakeMailtoEntry(TrustedProtocolEntry& entry);
	OP_STATUS MakeNntpEntry(TrustedProtocolEntry& entry);
	OP_STATUS MakeSourceViewerEntry(TrustedProtocolEntry& entry);

	OP_STATUS SaveMailtoEntry(const TrustedProtocolEntry& entry) const;
	OP_STATUS SaveSourceViewerEntry(const TrustedProtocolEntry& entry) const;

private:
	OpAutoVector<TrustedProtocolEntry>	m_list;
};


class TrustedProtocolDialog : public Dialog
{
public:
	TrustedProtocolDialog(TrustedProtocolList* list, INT32 index);

	virtual void				OnInit();
	void 						OnInitVisibility();
	BOOL						OnInputAction(OpInputAction* action);

	virtual BOOL				GetModality()			{return TRUE;}
	virtual BOOL				GetIsBlocking() 		{return TRUE;}
	virtual Type				GetType()				{return DIALOG_TYPE_TRUSTED_PROTOCOL;}
	virtual const char*			GetWindowName()			{return "Trusted Protocol Dialog";}

	INT32						GetButtonCount() { return 2; };

	void						OnChange(OpWidget *widget, BOOL changed_by_mouse);

private:
	TrustedProtocolList* m_list;
	INT32 m_index;
};


class WebFeedDialog : public Dialog
{
private:
	struct Item
	{
	public:
		Item(INT32 id):m_id(id),m_deleted(FALSE) {}

		INT32 m_id;
		BOOL m_deleted;
		OpString m_name;
	};

public:
	WebFeedDialog() {}

	void OnInit();
	BOOL OnInputAction(OpInputAction* action);

	virtual BOOL		 GetModality()	 {return TRUE;}
	virtual BOOL		 GetIsBlocking() {return TRUE;}
	virtual Type		 GetType()		 {return DIALOG_TYPE_TRUSTED_PROTOCOL;}
	virtual const char*	 GetWindowName() {return "Web Feed Dialog";}

private:
	OpAutoVector<Item> m_list;
};


#endif
