/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

/**
 * @file SecurityInformationDialog.h
 *
 * Dialog that is used to display security information (basically certificate details)
 *
 * Currently also contains the parser to extract the security information from the information url.
 */

#ifndef SECURITY_INFORMATION_DIALOG
#define SECURITY_INFORMATION_DIALOG

#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "adjunct/desktop_util/treemodel/simpletreemodel.h"
#ifdef TRUST_INFO_RESOLVER
# include "modules/windowcommander/OpTrustInfoResolver.h"
#endif // TRUST_INFO_RESOLVER
#ifdef SECURITY_INFORMATION_PARSER
class OpSecurityInformationParser;
class OpSecurityInformationContainer;
#endif // SECURITY_INFORMATION_PARSER
/**
 * The dialog that displays certificate and protocol information for secure connections.
 * Implements Dialog because that's what it is.
 * Implements OpPageListener because it needs to steal some of the browserview's input.
 */
class SecurityInformationDialog : public Dialog, OpPageListener
#ifdef TRUST_INFO_RESOLVER
	, public OpTrustInfoResolverListener
#endif // TRUST_INFO_RESOLVER
{
private:
	SimpleTreeModel*					m_server_tree_model;	///< Treemodel that populates the treeview.
	SimpleTreeModel*					m_client_tree_model;	///< Treemodel that populates the client cert chain treeview.
#ifdef SECURITY_INFORMATION_PARSER
	OpSecurityInformationParser*		m_parser;				///< The parser that extracts the information from the security information URL.
#endif // SECURITY_INFORMATION_PARSER
	OpDocumentListener::SecurityMode	m_security_mode;		///< The security mode.
	OpDocumentListener::TrustMode		m_trust_level;			///< The trust level.
	static const char *					PAGE_IMAGE_NAME_POSTFIX; ///< The postfix of the name of the image widgets shown on tabs. The start of the name is the text of the tab.
#ifdef SECURITY_INFORMATION_PARSER
	OpSecurityInformationContainer*		m_info_container;		///< The container that holds the security information strings.
#endif // SECURITY_INFORMATION_PARSER
	UINT32								m_low_security_reason;	///< The reson(s) for setting low security. Will not be set if the security hasn't been lowered.
	OpString							m_server_name;          ///< The server of the page we are requesting trust information for.
	OpWindowCommander*					m_wic;					///< Windowcommander managing the window that we're displaying sec info for.
	int									m_remembered_cert_pos;  ///< Postion of the remembered certificate in the certificate storage. -1 if not found.
#ifdef TRUST_INFO_RESOLVER
	OpTrustInformationResolver*			m_trust_info_resolver;	///< Helper class that handles the trust info resolving
#endif // TRUST_INFO_RESOLVER
	URLInformation *					m_url_info;

	// Group of functions broken out of OnInit for readability.
	/**
	 * Simple helper to make OnInit more readable
	 * @return Error if there is some sort of problem populating the page
	 */
	OP_STATUS PopulateTrustPage();
	/**
	 * Start download of trust information.
	 * In a separate function because the download can
	 * be initiated after the page was populated if
	 * the user has the feature off.
	 *
	 * Can also be used after resolving the host, when the ServerName
	 * is unresolved because it is loaded from cache. In that case
	 * the force_resolved argument must be set.
	 *
	 * @param force_resolved Set to true if calling this function from
	 */
	OP_STATUS DownloadTrustInformation(BOOL force_resolved = FALSE);
	/**
	 * Simple helper to make OnInit more readable.
	 * @return error if there is some sort of problem detecteed when populating the page.
	 */
	OP_STATUS PopulateCertificateDetailsPage();
	/**
	 * Simple helper to make OnInit more readable.
	 * @return error if there is some sort of problem detecteed when populating the page.
	 */
	OP_STATUS PopulateSummaryPage();

	// Internal functions (for convenience, readability and maintainability)
	/**
	 * Function that generates the initial dialog text based on security state and detected problems.
	 */
	void MakeDialogText(OpString &string);
	
	/**
	 * Checks whether there is a security issue that can be explained more specifically
	 * @return TRUE is there is a security problem with a known cause, FALSE if there is an unspecified or no security problem.
	 */
	BOOL HasSpecifiedSecurityIssue();
	
	// Dialog text building functions for specific purposes. Broken out of MakeDialogText for convenience.
	OP_STATUS AddRenegotiationWarningIfNeeded(OpString &string);
	OP_STATUS MakeExtendedValidationText(OpString &string);
	OP_STATUS MakeNormalSecurityText(OpString &string);
	OP_STATUS MakeFailedSecurityText(OpString &string);
	OP_STATUS MakeNoSecurityText(OpString &string);
	OP_STATUS MakeNoSecurityWithReservationsText(OpString &string);
	OP_STATUS MakeFraudWarningText(OpString &string);

	/**
	 * Function that makes the dialogs title string.
	 * @param the string that is to be filled with the title string.
	 */
	void MakeTitleString(OpString &string);

	/**
	 * Function that hides the certificate details information when they are not available.
	 */
	OP_STATUS HideCertificateDetails();

	/**
	 * Encapsulation of the work needed to set the content and state of a tree view widget in this dialog.
	 * @param name The name of the OpTreeView widget. If the widget is NOT an OpTreeView, the function will return error.
	 * @param tree_model A pointer to the tree model that this tree view is set with.
	 * @return ERR if the widget is not a OpTreeView or if the tree_model is NULL
	 */
	OP_STATUS SetTreeViewModel(const char* name, SimpleTreeModel* tree_model);

	/**
	 * Convenience function that sets the specified widget to label mode. Will only work for OpMultilineEdit widgets!
	 * If the content of the widget is larger than the size of the widget, it is not
	 * labelified, as that would make some of the content unreadable.
	 * @param name The name of the widget to labelify. Must be multilineedit
	 * @return OK if the widget was set to label mode, ERR otherwise.
	 */
	OP_STATUS LabelifyWidget(const char* name);

	/**
	 * Convenience. Sets visibility to false for the widget with the specified name.
	 * @param name of the widget to hide.
	 * @return OK if the widget was found, ERR if not.
	 */
	OP_STATUS HideWidget(const char* name);

	/**
	 * Convenience. Appends the string with the supplied id to the target, if desireable with a number first.
	 * Always starts with a line break.
	 * Example output:
	 *
	 * \n(1) Certificate expired
	 *
	 * @param number The number to insert at the front of the line.
	 * @param string_id The LocaleString to append
	 * @param target_string The string to append to. Will be changed.
	 * @return ERR if there was some sort of problem.
	 */
	OP_STATUS AppendNumbered(UINT number, Str::LocaleString string_id, OpString &target_string);

	/**
	 * Return the number of seconds until the certificate expires.
	 *
	 * @param expiry_time The expiry time in string format, shall accept m_expires-values
	 *
	 * @param (out) The number of seconds until the expiry date. 0 means the certificate expires this second, negative value means the certficate has expired already.
	 *
	 * @return OK if ok, ERR if error.
	 */
	OP_STATUS SecondsUntilExpiry(const OpStringC &expiry_time, long &secs_until);

	/**
	 * Return the tm structure representing the supplied "certificate format" time.
	 *
	 * @param time in string format as used in certificates
	 * @param timestruct tm struct that is to be filled in with the time information
	 * @return OK if the parsing went well, ERR otherwise.
	 */
	OP_STATUS GetTMStructTime(const OpStringC &time, tm &timestruct);

	/**
	 * Removed from MakeFailedSecurityText for readability.
	 * Count certificate errors/low security reasons.
	 * @return The number of errors found with the certificate.
	 */
	UINT CountErrosAndLowSecReasons();

	/**
	 * Call to display the trust info browserview and hide other widgets
	 * when the trust info is successfully downloaded.
	 */
	void TrustPageDownloadSuccess();
	/**
	 * Call to display trust info download error message, and hide
	 * other widgets.
	 */
	void TrustPageDownloadError();
	/**
	 * Call this to remove the current site from
	 * the remembered certificates store. If called
	 * from a site not remembered, it will do nothing.
	 * If the m_info_container isn't populated, it will
	 * do nothing. (as it needs it to get the name of
	 * the certificate.
	 */
	OP_STATUS RemoveFromRemembered();

public:
	/**
	 * Constructor
	 * @param wic window commander for the window in question.
	 */
						SecurityInformationDialog();
						~SecurityInformationDialog(); ///< Destructor.

	OP_STATUS			Init(DesktopWindow* parent_window, OpWindowCommander* wic, URLInformation * url_info = NULL);

#ifdef TRUST_RATING
	/**
	 * Setter for the trust rating member.
	 *
	 * @param trust_level The trust level to set.
	 *
	 * Setting this member would normally have been part of the construcor,
	 * but in order to make it reasonably easy to turn the feature in
	 * the preprocessor, it is factored out.
	 */
	inline void			SetTrustLevel(OpDocumentListener::TrustMode trust_rating) { m_trust_level = trust_rating; }
#endif // TRUST_RATING
	// Dialog overrides to customize behaviour.
	Type				GetType()				{return DIALOG_TYPE_SECURITY_INFORMATION_DIALOG;}
	const char*			GetWindowName()			{return "Security Information Dialog";}
	DialogType			GetDialogType()			{return TYPE_OK;}
	BOOL				GetShowPagesAsTabs()	{return TRUE;}
	const char*			GetHelpAnchor()			{return "fraudprotection.html";}
	BOOL				GetModality()			{return TRUE;}
	void				OnSetFocus()			{FocusButton(0);}
	void				OnInit();
	UINT32				OnOk();
	void				OnChange(OpWidget* widget, BOOL changed_by_mouse); ///< Handles selections from the treeview and displays relevant information.
	BOOL				OnInputAction(OpInputAction* action);
#ifndef TRUST_RATING
	const uni_char*     GetDialogImage();                                  ///< Dynamically specifies the icon based on the current security level.
#endif // !TRUST_RATING
	virtual BOOL        GetShowPage(INT32 page_number);                    ///< Disables the third tab if no client cert is present.

	BOOL				OnPagePopupMenu(OpWindowCommander* commander, OpDocumentContext& context);

						// OpPageListener API implementation
	void				OnPageLoadingProgress(OpWindowCommander* commander, const LoadingInformation* info);
	void				OnPageLoadingFinished(OpWindowCommander* commander, OpLoadingListener::LoadingFinishStatus status, BOOL stopped_by_user);

#ifdef TRUST_INFO_RESOLVER
						// TrustInfoResolverListener API implementation
	void				OnTrustInfoResolved();
	void				OnTrustInfoResolveError();
#endif // TRUST_INFO_RESOLVER
};

#endif // SECURITY_INFORMATION_DIALOG
